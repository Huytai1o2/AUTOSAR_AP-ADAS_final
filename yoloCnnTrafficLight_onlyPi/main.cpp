/*
 * Traffic Light Detector — Raspberry Pi 5
 *
 * Pipeline:
 *   1. LetterBox frame → 320×320, ghi lại transform params
 *   2. YOLO26n e2e int8 detect → boxes trong 320×320 space
 *   3. Inverse-letterbox → boxes trên ảnh gốc (full resolution)
 *   4. Vẽ boxes lên ảnh gốc
 *   5. Với mỗi direction có confidence cao nhất (left/right/straight):
 *        spawn 1 thread thực hiện:
 *          a. HSV detect màu đèn (red/yellow/green) từ ROI ảnh gốc
 *          b. Tìm timer box gần nhất trên ảnh gốc
 *          c. Crop timer ROI từ ảnh gốc (full-res → OCR rõ hơn)
 *          d. OCR đọc số đếm ngược
 *   6. Tổng hợp + annotate kết quả lên ảnh gốc
 *
 * YOLO: SetNumThreads(4) — all 4 Cortex-A76 cores
 * OCR : 1 thread/direction (≤3 threads song song, mỗi thread 1 core)
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/opencv.hpp>

#include "tensorflow/lite/core/c/common.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/interpreter_builder.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model_builder.h"

namespace fs = std::filesystem;

// ─────────────────────────── CONFIG ────────────────────────────────────────

static constexpr int   NC       = 4;      // left, right, straight, timer
static constexpr float CONF_THR = 0.4f;
static constexpr float IOU_THR  = 0.45f;
static constexpr int   OCR_W    = 96;
static constexpr int   OCR_H    = 64;

static const char* const TIMER_MODEL = "myCNN/timer_model.tflite";
static const char* const PIC_DIR     = "picTest";

static const char* const YOLO_CANDIDATES[] = {
    "best_yolo26n_e2e_480/best_int8.tflite",
    "best_yolo26n_e2e_480/best_float16.tflite",
    "best_yolo26n_e2e_480/best_float32.tflite",
    nullptr
};

// Class IDs
static constexpr int CLS_LEFT     = 0;
static constexpr int CLS_RIGHT    = 1;
static constexpr int CLS_STRAIGHT = 2;
static constexpr int CLS_TIMER    = 3;

static const char* const DIR_NAMES[3] = {"left", "right", "straight"};
static const int          DIR_CLS[3]  = {CLS_LEFT, CLS_RIGHT, CLS_STRAIGHT};

// ─────────────────────────── STRUCTURES ────────────────────────────────────

struct Box {
    float x1, y1, x2, y2, conf;
    int   cls;

    float cx()   const { return (x1 + x2) * 0.5f; }
    float cy()   const { return (y1 + y2) * 0.5f; }
    float area() const { return std::max(0.f,x2-x1) * std::max(0.f,y2-y1); }

    float distTo(const Box& o) const {
        float dx = cx()-o.cx(), dy = cy()-o.cy();
        return std::sqrt(dx*dx + dy*dy);
    }

    cv::Rect toRect() const {
        int x = static_cast<int>(x1), y = static_cast<int>(y1);
        int w = static_cast<int>(x2 - x1), h = static_cast<int>(y2 - y1);
        return {x, y, std::max(1,w), std::max(1,h)};
    }
};

// Tham số letterbox để inverse-map về ảnh gốc
struct LbInfo {
    float scale;
    int   pad_top, pad_left;
    int   orig_w,  orig_h;
    int   target;
};

// Kết quả mỗi direction thread
struct DirResult {
    const char* name;
    Box         dir_orig;      // direction box trên ảnh gốc
    Box         timer_orig;    // timer box trên ảnh gốc
    bool        has_timer = false;
    std::string color;         // "red" / "yellow" / "green" / "unknown"
    std::string timer_val;     // OCR result
};

// ─────────────────────────── NMS ───────────────────────────────────────────

static float boxIoU(const Box& a, const Box& b) {
    float ix1 = std::max(a.x1,b.x1), iy1 = std::max(a.y1,b.y1);
    float ix2 = std::min(a.x2,b.x2), iy2 = std::min(a.y2,b.y2);
    float inter = std::max(0.f,ix2-ix1) * std::max(0.f,iy2-iy1);
    float uni   = a.area() + b.area() - inter;
    return uni > 0.f ? inter/uni : 0.f;
}

static std::vector<Box> nms(std::vector<Box>& boxes, float iou_thr) {
    if (boxes.empty()) return {};
    std::vector<int> idx(boxes.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(),
              [&](int a, int b){ return boxes[a].conf > boxes[b].conf; });
    std::vector<char> sup(boxes.size(), 0);
    std::vector<Box>  keep;
    for (size_t ii = 0; ii < idx.size(); ++ii) {
        int i = idx[ii]; if (sup[i]) continue;
        keep.push_back(boxes[i]);
        for (size_t jj = ii+1; jj < idx.size(); ++jj) {
            int j = idx[jj]; if (sup[j]) continue;
            if (boxes[i].cls == boxes[j].cls && boxIoU(boxes[i],boxes[j]) > iou_thr)
                sup[j] = 1;
        }
    }
    return keep;
}

// ─────────────────────────── LETTERBOX ─────────────────────────────────────

static cv::Mat letterbox(const cv::Mat& img, int target, LbInfo& lb) {
    lb.scale    = std::min((float)target / img.rows, (float)target / img.cols);
    int new_h   = static_cast<int>(std::round(img.rows * lb.scale));
    int new_w   = static_cast<int>(std::round(img.cols * lb.scale));
    lb.pad_top  = (target - new_h) / 2;
    lb.pad_left = (target - new_w) / 2;
    lb.orig_h   = img.rows; lb.orig_w = img.cols;
    lb.target   = target;

    cv::Mat resized;
    cv::resize(img, resized, {new_w, new_h}, 0, 0, cv::INTER_LINEAR);
    cv::Mat out;
    cv::copyMakeBorder(resized, out,
                       lb.pad_top,  target - new_h - lb.pad_top,
                       lb.pad_left, target - new_w - lb.pad_left,
                       cv::BORDER_CONSTANT, cv::Scalar(0,0,0));
    return out;
}

// Map box từ padded pixel space về original image coords
static Box inverseMap(const Box& b, const LbInfo& lb) {
    auto mx = [&](float x) {
        float v = (x - lb.pad_left) / lb.scale;
        return std::max(0.f, std::min(v, static_cast<float>(lb.orig_w - 1)));
    };
    auto my = [&](float y) {
        float v = (y - lb.pad_top) / lb.scale;
        return std::max(0.f, std::min(v, static_cast<float>(lb.orig_h - 1)));
    };
    return {mx(b.x1), my(b.y1), mx(b.x2), my(b.y2), b.conf, b.cls};
}

// ─────────────────────────── HSV COLOR DETECT ──────────────────────────────
/*
 * Phát hiện màu đèn giao thông trong ROI BGR.
 * Trả về "red" / "yellow" / "green" / "unknown".
 * OpenCV HSV: H[0..179], S[0..255], V[0..255]
 */
static std::string detectColor(const cv::Mat& roi_bgr) {
    if (roi_bgr.empty()) return "unknown";
    cv::Mat hsv;
    cv::cvtColor(roi_bgr, hsv, cv::COLOR_BGR2HSV);

    // Red (hai dải, vì đỏ nằm ở đầu và cuối vòng màu)
    cv::Mat r1, r2;
    cv::inRange(hsv, cv::Scalar(  0, 100,  50), cv::Scalar( 10, 255, 255), r1);
    cv::inRange(hsv, cv::Scalar(155, 100,  50), cv::Scalar(179, 255, 255), r2);
    int n_red    = cv::countNonZero(r1 | r2);

    // Yellow
    cv::Mat ym;
    cv::inRange(hsv, cv::Scalar(18, 100, 100), cv::Scalar(35, 255, 255), ym);
    int n_yellow = cv::countNonZero(ym);

    // Green
    cv::Mat gm;
    cv::inRange(hsv, cv::Scalar(40, 50, 50), cv::Scalar(90, 255, 255), gm);
    int n_green  = cv::countNonZero(gm);

    if (n_red == 0 && n_yellow == 0 && n_green == 0) return "unknown";
    if (n_red >= n_yellow && n_red >= n_green)        return "red";
    if (n_yellow >= n_green)                          return "yellow";
    return "green";
}

// ─────────────────────────── DEQUANTIZE ────────────────────────────────────

static std::vector<float> dequantizeTensor(const TfLiteTensor* t) {
    int n = 1;
    for (int i = 0; i < t->dims->size; ++i) n *= t->dims->data[i];
    std::vector<float> out(n);

    if (t->type == kTfLiteFloat32) {
        const float* d = reinterpret_cast<const float*>(t->data.raw_const);
        std::copy(d, d+n, out.begin()); return out;
    }
    int dim1 = (t->dims->size>1) ? t->dims->data[1] : 1;
    int dim2 = (t->dims->size>2) ? t->dims->data[2] : 1;
    std::vector<float>   scales(dim1, t->params.scale);
    std::vector<int32_t> zeros(dim1,  t->params.zero_point);
    if (t->quantization.type == kTfLiteAffineQuantization) {
        const auto* aq = static_cast<const TfLiteAffineQuantization*>(
                             t->quantization.params);
        if (aq && aq->scale && aq->scale->size == dim1)
            for (int j=0; j<dim1; ++j) { scales[j]=aq->scale->data[j]; zeros[j]=aq->zero_point->data[j]; }
    }
    if (t->type == kTfLiteInt8) {
        const int8_t* d = reinterpret_cast<const int8_t*>(t->data.raw_const);
        for (int c=0; c<dim1; ++c) for (int k=0; k<dim2; ++k) {
            int i=c*dim2+k;
            out[i] = scales[c]*(static_cast<float>(d[i])-zeros[c]);
        }
    } else {
        const uint8_t* d = reinterpret_cast<const uint8_t*>(t->data.raw_const);
        for (int c=0; c<dim1; ++c) for (int k=0; k<dim2; ++k) {
            int i=c*dim2+k;
            out[i] = scales[c]*(static_cast<float>(d[i])-zeros[c]);
        }
    }
    return out;
}

static std::vector<float> dequantizePerTensor(const TfLiteTensor* t) {
    int n = 1;
    for (int i=0; i<t->dims->size; ++i) n *= t->dims->data[i];
    std::vector<float> out(n);
    if (t->type == kTfLiteFloat32) {
        const float* d = reinterpret_cast<const float*>(t->data.raw_const);
        std::copy(d, d+n, out.begin());
    } else if (t->type == kTfLiteInt8) {
        float sc=t->params.scale; int32_t zp=t->params.zero_point;
        const int8_t* d = reinterpret_cast<const int8_t*>(t->data.raw_const);
        for (int i=0; i<n; ++i) out[i]=sc*(static_cast<float>(d[i])-zp);
    } else {
        float sc=t->params.scale; int32_t zp=t->params.zero_point;
        const uint8_t* d = reinterpret_cast<const uint8_t*>(t->data.raw_const);
        for (int i=0; i<n; ++i) out[i]=sc*(static_cast<float>(d[i])-zp);
    }
    return out;
}

// ─────────────────────────── OCR DECODE ────────────────────────────────────

static std::string decodeOcr(const float* L, const float* S1,
                              const float* S2, const float* S3) {
    auto logc = [](float v){ return std::log(std::max(v,1e-10f)); };
    auto argmax10 = [&](const float* h) {
        int best=0; float bv=-1e30f;
        for (int i=0; i<10; ++i) { float v=logc(h[i]); if(v>bv){bv=v;best=i;} }
        return best;
    };
    int bs1=argmax10(S1), bs2=argmax10(S2), bs3=argmax10(S3);
    float s1=logc(L[0])+logc(S3[bs3]);
    float s2=logc(L[1])+logc(S2[bs2])+logc(S3[bs3]);
    float s3=logc(L[2])+logc(S1[bs1])+logc(S2[bs2])+logc(S3[bs3]);
    if (s1>=s2&&s1>=s3) return std::to_string(bs3);
    if (s2>=s3)         return std::to_string(bs2)+std::to_string(bs3);
    return std::to_string(bs1)+std::to_string(bs2)+std::to_string(bs3);
}

// ─────────────────────────── OCR HELPER ────────────────────────────────────

static std::string runOcr(const cv::Mat& roi_bgr,
                           const tflite::FlatBufferModel* model) {
    if (roi_bgr.empty()) return "ERR";

    tflite::ops::builtin::BuiltinOpResolver resolver;
    std::unique_ptr<tflite::Interpreter> interp;
    if (tflite::InterpreterBuilder(*model, resolver)(&interp) != kTfLiteOk || !interp)
        return "ERR";
    interp->SetNumThreads(1);
    if (interp->AllocateTensors() != kTfLiteOk) return "ERR";

    // Map outputs: L→3, S3→10, S1/S2→11 sorted by tensor index
    int out_L=-1, out_S1=-1, out_S2=-1, out_S3=-1;
    std::vector<std::pair<int,int>> h11;
    for (int s=0; s<(int)interp->outputs().size(); ++s) {
        const TfLiteTensor* t = interp->output_tensor(s);
        int nc = t->dims->data[t->dims->size-1];
        if      (nc==3)  out_L  = s;
        else if (nc==10) out_S3 = s;
        else if (nc==11) h11.push_back({interp->outputs()[s], s});
    }
    std::sort(h11.begin(), h11.end());
    if (h11.size()>=2) { out_S1=h11[0].second; out_S2=h11[1].second; }
    if (out_L<0||out_S1<0||out_S2<0||out_S3<0) return "ERR";

    // Preprocess: BGR → gray → resize → normalize
    cv::Mat gray, resized;
    cv::cvtColor(roi_bgr, gray, cv::COLOR_BGR2GRAY);
    cv::resize(gray, resized, {OCR_W, OCR_H}, 0, 0, cv::INTER_AREA);

    float* inp = interp->typed_input_tensor<float>(0);
    if (!inp) return "ERR";
    const float inv255 = 1.f/255.f;
    for (int r=0; r<OCR_H; ++r)
        for (int c=0; c<OCR_W; ++c)
            inp[r*OCR_W+c] = resized.at<uint8_t>(r,c)*inv255;

    if (interp->Invoke() != kTfLiteOk) return "ERR";

    auto vL  = dequantizePerTensor(interp->output_tensor(out_L));
    auto vS1 = dequantizePerTensor(interp->output_tensor(out_S1));
    auto vS2 = dequantizePerTensor(interp->output_tensor(out_S2));
    auto vS3 = dequantizePerTensor(interp->output_tensor(out_S3));
    return decodeOcr(vL.data(), vS1.data(), vS2.data(), vS3.data());
}

// ─────────────────────────── DIRECTION THREAD ──────────────────────────────
/*
 * Mỗi thread xử lý 1 direction:
 *   a. HSV detect màu đèn từ direction ROI trên ảnh gốc
 *   b. Tìm timer box gần nhất
 *   c. OCR từ timer ROI full-resolution
 */
static void dirWorker(DirResult& res,
                      const cv::Mat& orig,
                      const std::vector<Box>& timers_orig,
                      const tflite::FlatBufferModel* timerModel) {
    // a. Crop direction ROI từ ảnh gốc
    cv::Rect dir_rect = res.dir_orig.toRect();
    dir_rect &= cv::Rect(0, 0, orig.cols, orig.rows);   // clamp
    if (dir_rect.empty()) { res.color="unknown"; return; }
    res.color = detectColor(orig(dir_rect));

    // b. Tìm timer box gần nhất trên ảnh gốc
    if (timers_orig.empty()) return;
    float min_d = 1e30f;  int bt = -1;
    for (int j=0; j<(int)timers_orig.size(); ++j) {
        float d = res.dir_orig.distTo(timers_orig[j]);
        if (d < min_d) { min_d=d; bt=j; }
    }
    if (bt < 0) return;
    res.timer_orig = timers_orig[bt];
    res.has_timer  = true;

    // c. Crop timer ROI từ ảnh gốc (full resolution)
    cv::Rect tr = res.timer_orig.toRect();
    tr &= cv::Rect(0, 0, orig.cols, orig.rows);
    if (tr.empty()) return;
    res.timer_val = runOcr(orig(tr), timerModel);
}

// ─────────────────────────── YOLO DETECTOR ─────────────────────────────────

class YoloDetector {
public:
    YoloDetector() {
        for (int i=0; YOLO_CANDIDATES[i]!=nullptr; ++i) {
            const char* path = YOLO_CANDIDATES[i];
            auto m = tflite::FlatBufferModel::BuildFromFile(path);
            if (!m) { std::cout<<"  Skip (not found): "<<path<<"\n"; continue; }
            tflite::ops::builtin::BuiltinOpResolver resolver;
            std::unique_ptr<tflite::Interpreter> interp;
            if (tflite::InterpreterBuilder(*m,resolver)(&interp)!=kTfLiteOk||!interp) {
                std::cout<<"  Skip (build failed): "<<path<<"\n"; continue; }
            interp->SetNumThreads(4);
            if (interp->AllocateTensors()!=kTfLiteOk) {
                std::cout<<"  Skip (AllocateTensors): "<<path<<"\n"; continue; }
            model_=std::move(m); interp_=std::move(interp);
            std::cout<<"  [YOLO] Loaded: "<<path<<"\n"; break;
        }
        if (!interp_) throw std::runtime_error("No YOLO model loaded");

        const TfLiteTensor* inp = interp_->input_tensor(0);
        inp_type_=inp->type; inp_scale_=inp->params.scale; inp_zero_=inp->params.zero_point;
        img_size_=inp->dims->data[1];
        std::cout<<"  [YOLO] Input "<<img_size_<<"x"<<img_size_<<"  type="<<inp_type_<<"\n";

        const TfLiteTensor* out = interp_->output_tensor(0);
        int nd=out->dims->size, d1=out->dims->data[1];
        int d2=(nd>2)?out->dims->data[2]:0;
        std::cout<<"  [YOLO] Output: ";
        for (int i=0; i<nd; ++i) std::cout<<out->dims->data[i]<<" ";
        std::cout<<"\n";

        if (d1==(NC+4)&&d2>100)       { fmt_=FMT_RAW_CM; n_det_=d2; needs_nms_=true; }
        else if (d2==(NC+4)&&d1>100)  { fmt_=FMT_RAW_AM; n_det_=d1; needs_nms_=true; }
        else                           { fmt_=FMT_POST;   n_det_=d1; needs_nms_=false; }
        std::cout<<"  [YOLO] "<<(needs_nms_?"RAW+NMS":"end2end no-NMS")
                 <<" n_det="<<n_det_<<"\n";
    }

    int imgSize() const { return img_size_; }

    // Trả về boxes trong pixel space của padded image
    std::vector<Box> detect(const cv::Mat& padded_bgr) {
        using clk=std::chrono::steady_clock;
        using ms=std::chrono::duration<double,std::milli>;

        auto t0=clk::now();
        fillInput(padded_bgr);
        auto t1=clk::now();
        if (interp_->Invoke()!=kTfLiteOk) { std::cerr<<"  [YOLO] Invoke failed\n"; return {}; }
        auto t2=clk::now();
        auto result=parseOutput();
        auto t3=clk::now();
        std::cout<<"  [YOLO] pre="<<ms(t1-t0).count()
                 <<" ms  infer="<<ms(t2-t1).count()
                 <<" ms  post="<<ms(t3-t2).count()<<" ms\n";
        return result;
    }

private:
    enum Format { FMT_RAW_CM, FMT_RAW_AM, FMT_POST };

    void fillInput(const cv::Mat& bgr) {
        cv::Mat rgb; cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
        const int n_px=img_size_*img_size_*3;
        if (inp_type_==kTfLiteFloat32) {
            cv::Mat f32; rgb.convertTo(f32, CV_32FC3, 1.f/255.f);
            std::memcpy(interp_->typed_input_tensor<float>(0), f32.data, n_px*sizeof(float));
        } else if (inp_type_==kTfLiteInt8) {
            int8_t* d=interp_->typed_input_tensor<int8_t>(0);
            float sc=inp_scale_; int32_t zp=inp_zero_;
            for (int i=0; i<n_px; ++i) {
                int q=static_cast<int>(std::round(rgb.data[i]/255.f/sc))+zp;
                d[i]=static_cast<int8_t>(std::max(-128,std::min(127,q)));
            }
        } else {
            uint8_t* d=interp_->typed_input_tensor<uint8_t>(0);
            float sc=inp_scale_; int32_t zp=inp_zero_;
            for (int i=0; i<n_px; ++i) {
                int q=static_cast<int>(std::round(rgb.data[i]/255.f/sc))+zp;
                d[i]=static_cast<uint8_t>(std::max(0,std::min(255,q)));
            }
        }
    }

    std::vector<Box> parseOutput() {
        const TfLiteTensor* ot=interp_->output_tensor(0);
        std::vector<float> raw=dequantizeTensor(ot);
        std::vector<Box> proposals; proposals.reserve(64);
        float isz=static_cast<float>(img_size_);

        if (fmt_==FMT_RAW_CM) {
            float sample_cx=raw[0*n_det_+n_det_/2];
            float cscale=(std::abs(sample_cx)>1.5f)?1.f:isz;
            auto feat=[&](int a,int ch)->float{ return raw[ch*n_det_+a]; };
            for (int a=0; a<n_det_; ++a) {
                float ms=-1e30f; int cls=-1;
                for (int c=0; c<NC; ++c) { float s=feat(a,4+c); if(s>ms){ms=s;cls=c;} }
                if (ms<CONF_THR||cls<0||cls>3) continue;
                float cx=feat(a,0)*cscale, cy=feat(a,1)*cscale;
                float w =feat(a,2)*cscale, h =feat(a,3)*cscale;
                float x1=cx-w*.5f,y1=cy-h*.5f,x2=cx+w*.5f,y2=cy+h*.5f;
                x1=std::max(0.f,std::min(x1,isz)); y1=std::max(0.f,std::min(y1,isz));
                x2=std::max(0.f,std::min(x2,isz)); y2=std::max(0.f,std::min(y2,isz));
                if (x2>x1&&y2>y1) proposals.push_back({x1,y1,x2,y2,ms,cls});
            }
        } else if (fmt_==FMT_RAW_AM) {
            int stride=NC+4;
            float sample_cx=raw[(n_det_/2)*stride+0];
            float cscale=(std::abs(sample_cx)>1.5f)?1.f:isz;
            for (int a=0; a<n_det_; ++a) {
                const float* p=raw.data()+a*stride;
                float ms=-1e30f; int cls=-1;
                for (int c=0; c<NC; ++c) { if(p[4+c]>ms){ms=p[4+c];cls=c;} }
                if (ms<CONF_THR||cls<0||cls>3) continue;
                float cx=p[0]*cscale,cy=p[1]*cscale,w=p[2]*cscale,h=p[3]*cscale;
                float x1=cx-w*.5f,y1=cy-h*.5f,x2=cx+w*.5f,y2=cy+h*.5f;
                x1=std::max(0.f,std::min(x1,isz)); y1=std::max(0.f,std::min(y1,isz));
                x2=std::max(0.f,std::min(x2,isz)); y2=std::max(0.f,std::min(y2,isz));
                if (x2>x1&&y2>y1) proposals.push_back({x1,y1,x2,y2,ms,cls});
            }
        } else {
            // Post-processed (1, N, 6): normalised [0,1]
            for (int i=0; i<n_det_; ++i) {
                const float* p=raw.data()+i*6;
                if (p[4]<CONF_THR) continue;
                int cls=static_cast<int>(p[5]);
                if (cls<0||cls>3) continue;
                proposals.push_back({p[0]*isz,p[1]*isz,p[2]*isz,p[3]*isz,p[4],cls});
            }
        }
        if (needs_nms_) return nms(proposals, IOU_THR);
        return proposals;
    }

    std::unique_ptr<tflite::FlatBufferModel> model_;
    std::unique_ptr<tflite::Interpreter>     interp_;
    TfLiteType inp_type_  = kTfLiteFloat32;
    float      inp_scale_ = 1.f;
    int32_t    inp_zero_  = 0;
    Format     fmt_       = FMT_POST;
    int        n_det_     = 300;
    int        img_size_  = 320;
    bool       needs_nms_ = false;
};

// ─────────────────────────── DRAW HELPERS ──────────────────────────────────

static cv::Scalar colorOf(const std::string& c) {
    if (c=="green")  return {0,255,0};
    if (c=="yellow") return {0,255,255};
    if (c=="red")    return {0,0,255};
    return {180,180,180};
}

static void drawBox(cv::Mat& img, const Box& b,
                    const cv::Scalar& col, const std::string& label) {
    cv::rectangle(img, b.toRect(), col, 2);
    if (!label.empty()) {
        int bl=0;
        cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &bl);
        int lx = static_cast<int>(b.x1);
        int ly = std::max(0, static_cast<int>(b.y1) - ts.height - 4);
        cv::rectangle(img, {lx, ly}, {lx+ts.width+4, ly+ts.height+4}, col, cv::FILLED);
        cv::putText(img, label, {lx+2, ly+ts.height+1},
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, {0,0,0}, 1);
    }
}

// ─────────────────────────── PROCESS FRAME ─────────────────────────────────

static cv::Mat processFrame(const cv::Mat& frame,
                            YoloDetector& yolo,
                            const tflite::FlatBufferModel* timerModel) {
    using clk=std::chrono::steady_clock;
    using ms=std::chrono::duration<double,std::milli>;

    // 1. Letterbox (ghi lại transform)
    auto t0=clk::now();
    LbInfo lb;
    cv::Mat padded = letterbox(frame, yolo.imgSize(), lb);
    std::cout<<"  [letterbox "<<lb.target<<"x"<<lb.target<<"]  "
             <<ms(clk::now()-t0).count()<<" ms\n";

    // 2. YOLO detect → boxes trong 320×320 pixel space
    std::vector<Box> boxes_pad = yolo.detect(padded);

    // 3. Inverse-map → boxes trên ảnh gốc
    std::vector<Box> boxes_orig;
    boxes_orig.reserve(boxes_pad.size());
    for (const auto& b : boxes_pad)
        boxes_orig.push_back(inverseMap(b, lb));

    // 4. Chọn best-confidence per direction + collect timers (trên ảnh gốc)
    //    Chỉ lấy 1 box tốt nhất mỗi hướng để nhét vào pipeline
    Box   best[3];
    float best_conf[3]={-1,-1,-1};
    bool  has[3]={false,false,false};
    std::vector<Box> timers;

    for (const auto& b : boxes_orig) {
        if (b.cls==CLS_TIMER) { timers.push_back(b); continue; }
        for (int s=0; s<3; ++s) {
            if (b.cls==DIR_CLS[s] && (!has[s]||b.conf>best_conf[s])) {
                best[s]=b; best_conf[s]=b.conf; has[s]=true; break;
            }
        }
    }

    cv::Mat result = frame.clone();

    // 6. Build DirResult tasks — chỉ best per direction
    std::vector<DirResult> tasks;
    for (int s=0; s<3; ++s) {
        if (!has[s]) continue;
        DirResult r;
        r.name     = DIR_NAMES[s];
        r.dir_orig = best[s];
        tasks.push_back(std::move(r));
    }

    // 7. Parallel: mỗi thread xử lý 1 direction
    {
        auto t_par=clk::now();
        std::vector<std::thread> threads;
        threads.reserve(tasks.size());
        for (auto& task : tasks)
            threads.emplace_back(dirWorker, std::ref(task),
                                 std::cref(frame), std::cref(timers), timerModel);
        for (auto& th : threads) th.join();
        std::cout<<"  [parallel x"<<tasks.size()<<"]  "
                 <<ms(clk::now()-t_par).count()<<" ms\n";
    }

    // 8. Overlay kết quả pipeline + log rõ màu
    for (const auto& r : tasks) {
        cv::Scalar light_col = colorOf(r.color);

        // Đè lại direction box bằng màu đèn thực tế
        char dlbl[64];
        std::snprintf(dlbl, sizeof(dlbl), "%s | %s | conf=%.2f",
                      r.name, r.color.c_str(), r.dir_orig.conf);
        drawBox(result, r.dir_orig, light_col, dlbl);

        // Log rõ ràng
        std::cout << "  ┌─ Direction : " << r.name << "\n"
                  << "  │  Color     : " << r.color << "\n"
                  << "  │  Conf      : " << r.dir_orig.conf << "\n";

        if (r.has_timer) {
            char tlbl[32];
            std::snprintf(tlbl, sizeof(tlbl), "timer: %s", r.timer_val.c_str());
            drawBox(result, r.timer_orig, {0,0,200}, tlbl);
            std::cout << "  └─ Timer     : " << r.timer_val << "\n";
        } else {
            std::cout << "  └─ Timer     : (not found)\n";
        }
        std::cout << "\n";
    }

    if (tasks.empty()) std::cout<<"  (no detections)\n";
    return result;
}

// ─────────────────────────── MAIN ──────────────────────────────────────────

int main() {
    std::cout<<"[*] Loading YOLO (int8→fp16→fp32)...\n";
    YoloDetector yolo;

    std::cout<<"[*] Loading timer OCR: "<<TIMER_MODEL<<"\n";
    auto timerModel = tflite::FlatBufferModel::BuildFromFile(TIMER_MODEL);
    if (!timerModel) { std::cerr<<"[!] Cannot load timer model\n"; return 1; }

    std::vector<fs::path> images;
    for (const auto& e : fs::directory_iterator(PIC_DIR)) {
        std::string ext = e.path().extension().string();
        for (char& c : ext) c=static_cast<char>(std::tolower(c));
        if (ext==".jpg"||ext==".jpeg"||ext==".png") images.push_back(e.path());
    }
    std::sort(images.begin(), images.end());
    if (images.empty()) { std::cerr<<"[!] No images in "<<PIC_DIR<<"\n"; return 1; }
    std::cout<<"[*] "<<images.size()<<" image(s) in "<<PIC_DIR<<"\n\n";

    for (const fs::path& p : images) {
        std::cout<<"[+] "<<p.filename().string()<<"\n";
        cv::Mat frame = cv::imread(p.string());
        if (frame.empty()) { std::cerr<<"    Cannot read — skip\n"; continue; }

        auto t0=std::chrono::steady_clock::now();
        cv::Mat out = processFrame(frame, yolo, timerModel.get());
        double ms=std::chrono::duration<double,std::milli>(
                      std::chrono::steady_clock::now()-t0).count();

        std::string dst="result_"+p.filename().string();
        cv::imwrite(dst, out);
        std::cout<<"  -> "<<dst<<"  (total "<<ms<<" ms)\n\n";
    }
    std::cout<<"[*] Done.\n";
    return 0;
}
