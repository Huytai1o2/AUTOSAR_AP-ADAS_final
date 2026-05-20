///////////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// Copyright, 2021 PopcornSAR Co., Ltd. All rights reserved.
///
///////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "providersensor/aa/providersensor.h"

#include <cmath>
#include <cstring>
#include <sched.h>

// ─────────────────────────── CONFIG ────────────────────────────────────────

#define GPS_UART_DEV      "/dev/ttyAMA0"
#define GPS_BAUD          B9600
#define GPS_BUF_SIZE      1024
#define GPS_TIMEOUT_S     3
#define GPS_RETRY_DELAY   2
#define GEOJSON_FILE_NAME "hochiminh.geojson"
#define RANGE_SIZE        200   // metres

static constexpr int   NC        = 4;      // left, right, straight, timer
static constexpr float CONF_THR  = 0.32f;
static constexpr float IOU_THR   = 0.45f;
static constexpr int   OCR_W     = 96;
static constexpr int   OCR_H     = 64;

static const char* const TIMER_MODEL    = "myCNN/timer_model.tflite";
static const char* const VIDEO_FALLBACK = "test.mp4";

// Hardcoded GPS fallback used when UART is unavailable (no hardware)
static constexpr double GPS_FALLBACK_LAT  = 10.7769;   // Ho Chi Minh City center
static constexpr double GPS_FALLBACK_LON  = 106.7009;
static constexpr int    GPS_MAX_RETRIES   = 3;          // attempts before switching to fixed location

// Tried in order; first loadable wins (416 preferred for speed)
static const char* const YOLO_CANDIDATES[] = {
    "best_yolo26n_e2e_416/best_int8.tflite",
    "best_yolo26n_e2e_416/best_float16.tflite",
    "best_yolo26n_e2e_416/best_float32.tflite",
    nullptr
};

static constexpr int CLS_LEFT     = 0;
static constexpr int CLS_RIGHT    = 1;
static constexpr int CLS_STRAIGHT = 2;
static constexpr int CLS_TIMER    = 3;

static const char* const DIR_NAMES[3] = {"left", "right", "straight"};
static const int          DIR_CLS[3]  = {CLS_LEFT, CLS_RIGHT, CLS_STRAIGHT};

// ─────────────────────────── WIRE FORMAT ───────────────────────────────────
// GPS metadata + JPEG packed into one DDS sample: [GpsJpegWireHeader][jpeg bytes]

namespace
{
#pragma pack(push, 1)
struct GpsJpegWireHeader
{
    std::uint32_t magic;
    std::uint8_t  version;
    std::uint8_t  in_range;
    std::uint8_t  reserved[2];
    std::int64_t  node_id;
    std::int32_t  dist_m;
    std::uint32_t jpeg_len;
};
#pragma pack(pop)

static_assert(sizeof(GpsJpegWireHeader) == 24, "GpsJpegWireHeader wire size");
constexpr std::uint32_t kGpsJpegMagic = 0x47505331u; // 'G''P''S''1'
} // namespace

namespace providersensor
{
namespace aa
{

// ─────────────────────────── YOLO DETECTOR ─────────────────────────────────
/*
 * Encapsulates model loading, input filling, and output parsing for
 * YOLO26n end-to-end (no NMS) and raw-head variants.
 *
 * Format auto-detection at construction:
 *   FMT_RAW_CM : output (NC+4, N) — needs_nms=true
 *   FMT_RAW_AM : output (N, NC+4) — needs_nms=true
 *   FMT_POST   : output (N,  6)   — needs_nms=false  ← yolo26n e2e
 *
 * For the end-to-end model with (1,300,6), d1=300 d2=6:
 *   d1 ≠ NC+4(=8) → skip FMT_RAW_CM
 *   d2 ≠ NC+4(=8) → skip FMT_RAW_AM
 *   → FMT_POST, needs_nms_=false, proposals returned as-is.
 */
class YoloDetector {
public:
    YoloDetector() {
        for (int i = 0; YOLO_CANDIDATES[i] != nullptr; ++i) {
            auto m = tflite::FlatBufferModel::BuildFromFile(YOLO_CANDIDATES[i]);
            if (!m) continue;
            tflite::ops::builtin::BuiltinOpResolver resolver;
            std::unique_ptr<tflite::Interpreter> interp;
            if (tflite::InterpreterBuilder(*m, resolver)(&interp) != kTfLiteOk || !interp) continue;
            interp->SetNumThreads(3);   // reserve 1 core for display/OS
            if (interp->AllocateTensors() != kTfLiteOk) continue;
            model_  = std::move(m);
            interp_ = std::move(interp);
            break;
        }
        if (!interp_) throw std::runtime_error("No YOLO model loaded");

        const TfLiteTensor* inp = interp_->input_tensor(0);
        inp_type_  = inp->type;
        inp_scale_ = inp->params.scale;
        inp_zero_  = inp->params.zero_point;
        img_size_  = inp->dims->data[1];

        const TfLiteTensor* out = interp_->output_tensor(0);
        int nd = out->dims->size;
        int d1 = out->dims->data[1];
        int d2 = (nd > 2) ? out->dims->data[2] : 0;

        if      (d1 == (NC+4) && d2 > 100) { fmt_ = FMT_RAW_CM; n_det_ = d2; needs_nms_ = true;  }
        else if (d2 == (NC+4) && d1 > 100) { fmt_ = FMT_RAW_AM; n_det_ = d1; needs_nms_ = true;  }
        else                                { fmt_ = FMT_POST;   n_det_ = d1; needs_nms_ = false; }
    }

    struct TimingMs { double pre, infer, post; };

    int imgSize()             const { return img_size_; }
    const TimingMs& timing()  const { return timing_; }

    std::vector<Box> detect(const cv::Mat& padded_bgr) {
        using clk = std::chrono::steady_clock;
        using ms  = std::chrono::duration<double, std::milli>;

        auto t0 = clk::now();
        fillInput(padded_bgr);
        auto t1 = clk::now();
        if (interp_->Invoke() != kTfLiteOk) return {};
        auto t2 = clk::now();
        auto result = parseOutput();
        auto t3 = clk::now();

        timing_ = { ms(t1-t0).count(), ms(t2-t1).count(), ms(t3-t2).count() };
        return result;
    }

private:
    enum Format { FMT_RAW_CM, FMT_RAW_AM, FMT_POST };

    // Per-channel or per-tensor dequantize depending on quantization metadata
    static std::vector<float> dequantize(const TfLiteTensor* t) {
        int n = 1;
        for (int i = 0; i < t->dims->size; ++i) n *= t->dims->data[i];
        std::vector<float> out(n);

        if (t->type == kTfLiteFloat32) {
            const float* d = reinterpret_cast<const float*>(t->data.raw_const);
            std::copy(d, d+n, out.begin());
            return out;
        }
        int dim1 = (t->dims->size > 1) ? t->dims->data[1] : 1;
        int dim2 = (t->dims->size > 2) ? t->dims->data[2] : 1;
        std::vector<float>   scales(dim1, t->params.scale);
        std::vector<int32_t> zeros(dim1,  t->params.zero_point);
        if (t->quantization.type == kTfLiteAffineQuantization) {
            const auto* aq = static_cast<const TfLiteAffineQuantization*>(t->quantization.params);
            if (aq && aq->scale && aq->scale->size == dim1)
                for (int j = 0; j < dim1; ++j) {
                    scales[j] = aq->scale->data[j];
                    zeros[j]  = aq->zero_point->data[j];
                }
        }
        if (t->type == kTfLiteInt8) {
            const int8_t* d = reinterpret_cast<const int8_t*>(t->data.raw_const);
            for (int c=0;c<dim1;++c) for(int k=0;k<dim2;++k){int i=c*dim2+k;out[i]=scales[c]*(static_cast<float>(d[i])-zeros[c]);}
        } else {
            const uint8_t* d = reinterpret_cast<const uint8_t*>(t->data.raw_const);
            for (int c=0;c<dim1;++c) for(int k=0;k<dim2;++k){int i=c*dim2+k;out[i]=scales[c]*(static_cast<float>(d[i])-zeros[c]);}
        }
        return out;
    }

    void fillInput(const cv::Mat& bgr) {
        cv::Mat rgb; cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
        const int n_px = img_size_ * img_size_ * 3;
        if (inp_type_ == kTfLiteFloat32) {
            cv::Mat f32; rgb.convertTo(f32, CV_32FC3, 1.f/255.f);
            std::memcpy(interp_->typed_input_tensor<float>(0), f32.data, n_px*sizeof(float));
        } else if (inp_type_ == kTfLiteInt8) {
            int8_t* d = interp_->typed_input_tensor<int8_t>(0);
            float sc = inp_scale_; int32_t zp = inp_zero_;
            for (int i = 0; i < n_px; ++i) {
                int q = static_cast<int>(std::round(rgb.data[i]/255.f/sc)) + zp;
                d[i] = static_cast<int8_t>(std::max(-128, std::min(127, q)));
            }
        } else {
            uint8_t* d = interp_->typed_input_tensor<uint8_t>(0);
            float sc = inp_scale_; int32_t zp = inp_zero_;
            for (int i = 0; i < n_px; ++i) {
                int q = static_cast<int>(std::round(rgb.data[i]/255.f/sc)) + zp;
                d[i] = static_cast<uint8_t>(std::max(0, std::min(255, q)));
            }
        }
    }

    static float iou(const Box& a, const Box& b) {
        float ix1=std::max(a.x1,b.x1), iy1=std::max(a.y1,b.y1);
        float ix2=std::min(a.x2,b.x2), iy2=std::min(a.y2,b.y2);
        float inter=std::max(0.f,ix2-ix1)*std::max(0.f,iy2-iy1);
        float uni=a.area()+b.area()-inter;
        return uni>0.f?inter/uni:0.f;
    }

    static std::vector<Box> nms(std::vector<Box>& boxes, float thr) {
        if (boxes.empty()) return {};
        std::vector<int> idx(boxes.size());
        std::iota(idx.begin(),idx.end(),0);
        std::sort(idx.begin(),idx.end(),[&](int a,int b){return boxes[a].conf>boxes[b].conf;});
        std::vector<char> sup(boxes.size(),0);
        std::vector<Box> keep;
        for (size_t ii=0;ii<idx.size();++ii){
            int i=idx[ii]; if(sup[i]) continue;
            keep.push_back(boxes[i]);
            for (size_t jj=ii+1;jj<idx.size();++jj){
                int j=idx[jj]; if(sup[j]) continue;
                if (boxes[i].cls==boxes[j].cls&&iou(boxes[i],boxes[j])>thr) sup[j]=1;
            }
        }
        return keep;
    }

    std::vector<Box> parseOutput() {
        const TfLiteTensor* ot = interp_->output_tensor(0);
        std::vector<float> raw = dequantize(ot);
        std::vector<Box> proposals;
        float isz = static_cast<float>(img_size_);

        if (fmt_ == FMT_RAW_CM) {
            float sample_cx = raw[0*n_det_ + n_det_/2];
            float cscale = (std::abs(sample_cx) > 1.5f) ? 1.f : isz;
            auto feat = [&](int a, int ch){ return raw[ch*n_det_+a]; };
            for (int a=0;a<n_det_;++a){
                float ms=-1e30f; int cls=-1;
                for (int c=0;c<NC;++c){float s=feat(a,4+c);if(s>ms){ms=s;cls=c;}}
                if (ms<CONF_THR||cls<0||cls>3) continue;
                float cx=feat(a,0)*cscale,cy=feat(a,1)*cscale;
                float w=feat(a,2)*cscale,h=feat(a,3)*cscale;
                float x1=cx-w*.5f,y1=cy-h*.5f,x2=cx+w*.5f,y2=cy+h*.5f;
                x1=std::max(0.f,std::min(x1,isz));y1=std::max(0.f,std::min(y1,isz));
                x2=std::max(0.f,std::min(x2,isz));y2=std::max(0.f,std::min(y2,isz));
                if (x2>x1&&y2>y1) proposals.push_back({x1,y1,x2,y2,ms,cls});
            }
        } else if (fmt_ == FMT_RAW_AM) {
            int stride = NC+4;
            float sample_cx = raw[(n_det_/2)*stride+0];
            float cscale = (std::abs(sample_cx) > 1.5f) ? 1.f : isz;
            for (int a=0;a<n_det_;++a){
                const float* p=raw.data()+a*stride;
                float ms=-1e30f; int cls=-1;
                for (int c=0;c<NC;++c){if(p[4+c]>ms){ms=p[4+c];cls=c;}}
                if (ms<CONF_THR||cls<0||cls>3) continue;
                float cx=p[0]*cscale,cy=p[1]*cscale,w=p[2]*cscale,h=p[3]*cscale;
                float x1=cx-w*.5f,y1=cy-h*.5f,x2=cx+w*.5f,y2=cy+h*.5f;
                x1=std::max(0.f,std::min(x1,isz));y1=std::max(0.f,std::min(y1,isz));
                x2=std::max(0.f,std::min(x2,isz));y2=std::max(0.f,std::min(y2,isz));
                if (x2>x1&&y2>y1) proposals.push_back({x1,y1,x2,y2,ms,cls});
            }
        } else {
            // FMT_POST: end-to-end model, output normalised [0,1] — no NMS needed
            for (int i=0;i<n_det_;++i){
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
    TimingMs   timing_    = {};
    TfLiteType inp_type_  = kTfLiteFloat32;
    float      inp_scale_ = 1.f;
    int32_t    inp_zero_  = 0;
    Format     fmt_       = FMT_POST;
    int        n_det_     = 300;
    int        img_size_  = 480;
    bool       needs_nms_ = false;
};

// ─────────────────────────── CONSTRUCTOR / DESTRUCTOR ──────────────────────

ProviderSensor::ProviderSensor()
    : m_logger(ara::log::CreateLogger("pSen", "SWC", ara::log::LogLevel::kVerbose))
    , m_running{false}
    , m_workers(3)
{}

ProviderSensor::~ProviderSensor() = default;

// ─────────────────────────── GPS SEQLOCK ───────────────────────────────────

void ProviderSensor::publishGpsSnapshot(std::int64_t nodeId, std::int32_t distM, std::uint8_t inRange)
{
    m_gpsSeq.fetch_add(1, std::memory_order_acq_rel);   // odd → locked
    m_gpsSnapshot.nodeId  = nodeId;
    m_gpsSnapshot.distM   = distM;
    m_gpsSnapshot.inRange = inRange;
    m_gpsSeq.fetch_add(1, std::memory_order_release);   // even → unlocked
}

bool ProviderSensor::tryCopyGpsSnapshot(GpsSnapshot& out) const
{
    for (int i = 0; i < 8; ++i) {
        std::uint32_t s1 = m_gpsSeq.load(std::memory_order_acquire);
        if (s1 & 1u) continue;
        out = m_gpsSnapshot;
        std::uint32_t s2 = m_gpsSeq.load(std::memory_order_acquire);
        if (s1 == s2 && (s2 & 1u) == 0) return true;
    }
    out = m_gpsSnapshot;   // best-effort fallback
    return true;
}

// ─────────────────────────── LETTERBOX / INVERSE-MAP ───────────────────────

cv::Mat ProviderSensor::letterbox(const cv::Mat& img, int target, LbInfo& lb)
{
    lb.scale    = std::min((float)target/img.rows, (float)target/img.cols);
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
                       lb.pad_top,  target-new_h-lb.pad_top,
                       lb.pad_left, target-new_w-lb.pad_left,
                       cv::BORDER_CONSTANT, cv::Scalar(0,0,0));
    return out;
}

Box ProviderSensor::inverseMap(const Box& b, const LbInfo& lb)
{
    auto mx=[&](float x){
        float v=(x-lb.pad_left)/lb.scale;
        return std::max(0.f,std::min(v,static_cast<float>(lb.orig_w-1)));
    };
    auto my=[&](float y){
        float v=(y-lb.pad_top)/lb.scale;
        return std::max(0.f,std::min(v,static_cast<float>(lb.orig_h-1)));
    };
    return {mx(b.x1),my(b.y1),mx(b.x2),my(b.y2),b.conf,b.cls};
}

// ─────────────────────────── HSV COLOR DETECT ──────────────────────────────

std::string ProviderSensor::detectColor(const cv::Mat& roi_bgr)
{
    if (roi_bgr.empty()) return "unknown";
    cv::Mat hsv;
    cv::cvtColor(roi_bgr, hsv, cv::COLOR_BGR2HSV);
    cv::Mat r1, r2;
    cv::inRange(hsv, cv::Scalar(  0,100, 50), cv::Scalar( 10,255,255), r1);
    cv::inRange(hsv, cv::Scalar(155,100, 50), cv::Scalar(179,255,255), r2);
    int n_red    = cv::countNonZero(r1|r2);
    cv::Mat ym;
    cv::inRange(hsv, cv::Scalar(18,100,100), cv::Scalar(35,255,255), ym);
    int n_yellow = cv::countNonZero(ym);
    cv::Mat gm;
    cv::inRange(hsv, cv::Scalar(40,50,50), cv::Scalar(90,255,255), gm);
    int n_green  = cv::countNonZero(gm);
    if (n_red==0&&n_yellow==0&&n_green==0) return "unknown";
    if (n_red>=n_yellow&&n_red>=n_green)   return "red";
    if (n_yellow>=n_green)                 return "yellow";
    return "green";
}

// ─────────────────────────── DEQUANTIZE (OCR) ──────────────────────────────

std::vector<float> ProviderSensor::dequantizePerTensor(const TfLiteTensor* t)
{
    int n=1;
    for (int i=0;i<t->dims->size;++i) n*=t->dims->data[i];
    std::vector<float> out(n);
    if (t->type==kTfLiteFloat32){
        const float* d=reinterpret_cast<const float*>(t->data.raw_const);
        std::copy(d,d+n,out.begin());
    } else if (t->type==kTfLiteInt8){
        float sc=t->params.scale; int32_t zp=t->params.zero_point;
        const int8_t* d=reinterpret_cast<const int8_t*>(t->data.raw_const);
        for (int i=0;i<n;++i) out[i]=sc*(static_cast<float>(d[i])-zp);
    } else {
        float sc=t->params.scale; int32_t zp=t->params.zero_point;
        const uint8_t* d=reinterpret_cast<const uint8_t*>(t->data.raw_const);
        for (int i=0;i<n;++i) out[i]=sc*(static_cast<float>(d[i])-zp);
    }
    return out;
}

// ─────────────────────────── OCR ───────────────────────────────────────────

std::string ProviderSensor::decodeOcr(const float* L, const float* S1,
                                       const float* S2, const float* S3)
{
    auto logc=[](float v){ return std::log(std::max(v,1e-10f)); };
    auto argmax10=[&](const float* h){
        int best=0; float bv=-1e30f;
        for (int i=0;i<10;++i){float v=logc(h[i]);if(v>bv){bv=v;best=i;}}
        return best;
    };
    int bs1=argmax10(S1),bs2=argmax10(S2),bs3=argmax10(S3);
    float s1=logc(L[0])+logc(S3[bs3]);
    float s2=logc(L[1])+logc(S2[bs2])+logc(S3[bs3]);
    float s3=logc(L[2])+logc(S1[bs1])+logc(S2[bs2])+logc(S3[bs3]);
    if (s1>=s2&&s1>=s3) return std::to_string(bs3);
    if (s2>=s3)         return std::to_string(bs2)+std::to_string(bs3);
    return std::to_string(bs1)+std::to_string(bs2)+std::to_string(bs3);
}

std::string ProviderSensor::runOcr(const cv::Mat& roi_bgr,
                                    const tflite::FlatBufferModel* model)
{
    if (roi_bgr.empty()) return "ERR";
    tflite::ops::builtin::BuiltinOpResolver resolver;
    std::unique_ptr<tflite::Interpreter> interp;
    if (tflite::InterpreterBuilder(*model,resolver)(&interp)!=kTfLiteOk||!interp) return "ERR";
    interp->SetNumThreads(1);
    if (interp->AllocateTensors()!=kTfLiteOk) return "ERR";

    // Identify outputs by size: L→3 classes, S3→10 digits, S1/S2→11 (ordered by tensor index)
    int out_L=-1, out_S1=-1, out_S2=-1, out_S3=-1;
    std::vector<std::pair<int,int>> h11;
    for (int s=0;s<(int)interp->outputs().size();++s){
        const TfLiteTensor* t=interp->output_tensor(s);
        int nc=t->dims->data[t->dims->size-1];
        if      (nc==3)  out_L=s;
        else if (nc==10) out_S3=s;
        else if (nc==11) h11.push_back({interp->outputs()[s],s});
    }
    std::sort(h11.begin(),h11.end());
    if (h11.size()>=2){out_S1=h11[0].second;out_S2=h11[1].second;}
    if (out_L<0||out_S1<0||out_S2<0||out_S3<0) return "ERR";

    cv::Mat gray, resized;
    cv::cvtColor(roi_bgr,gray,cv::COLOR_BGR2GRAY);
    cv::resize(gray,resized,{OCR_W,OCR_H},0,0,cv::INTER_AREA);
    float* inp=interp->typed_input_tensor<float>(0);
    if (!inp) return "ERR";
    const float inv255=1.f/255.f;
    for (int r=0;r<OCR_H;++r)
        for (int c=0;c<OCR_W;++c)
            inp[r*OCR_W+c]=resized.at<uint8_t>(r,c)*inv255;

    if (interp->Invoke()!=kTfLiteOk) return "ERR";
    auto vL =dequantizePerTensor(interp->output_tensor(out_L));
    auto vS1=dequantizePerTensor(interp->output_tensor(out_S1));
    auto vS2=dequantizePerTensor(interp->output_tensor(out_S2));
    auto vS3=dequantizePerTensor(interp->output_tensor(out_S3));
    return decodeOcr(vL.data(),vS1.data(),vS2.data(),vS3.data());
}

// ─────────────────────────── DIRECTION WORKER ──────────────────────────────

void ProviderSensor::dirWorker(DirResult& res,
                                const cv::Mat& orig,
                                const std::vector<Box>& timers_orig,
                                const tflite::FlatBufferModel* timerModel)
{
    cv::Rect dir_rect=res.dir_orig.toRect();
    dir_rect &= cv::Rect(0,0,orig.cols,orig.rows);
    if (dir_rect.empty()){res.color="unknown";return;}
    res.color=detectColor(orig(dir_rect));

    if (timers_orig.empty()) return;
    float min_d=1e30f; int bt=-1;
    for (int j=0;j<(int)timers_orig.size();++j){
        float d=res.dir_orig.distTo(timers_orig[j]);
        if (d<min_d){min_d=d;bt=j;}
    }
    if (bt<0) return;
    res.timer_orig=timers_orig[bt];
    res.has_timer=true;
    cv::Rect tr=res.timer_orig.toRect();
    tr &= cv::Rect(0,0,orig.cols,orig.rows);
    if (tr.empty()) return;
    res.timer_val=runOcr(orig(tr),timerModel);
}

// ─────────────────────────── DRAW HELPERS ──────────────────────────────────

cv::Scalar ProviderSensor::colorOf(const std::string& c)
{
    if (c=="green")  return {0,255,0};
    if (c=="yellow") return {0,255,255};
    if (c=="red")    return {0,0,255};
    return {180,180,180};
}

void ProviderSensor::drawBox(cv::Mat& img, const Box& b,
                              const cv::Scalar& col, const std::string& label)
{
    // Scale box and font relative to image width (reference: 640px)
    const double sc        = static_cast<double>(img.cols) / 640.0;
    const int    boxThick  = std::max(1, static_cast<int>(std::round(2.0 * sc)));
    const double fontScale = std::max(0.2, 0.5 * sc);
    const int    fontThick = std::max(1, static_cast<int>(std::round(fontScale * 2.0)));

    cv::rectangle(img, b.toRect(), col, boxThick, cv::LINE_AA);
    if (!label.empty()){
        int bl=0;
        cv::Size ts=cv::getTextSize(label,cv::FONT_HERSHEY_SIMPLEX,fontScale,fontThick,&bl);
        int lx=static_cast<int>(b.x1);
        int ly=std::max(0,static_cast<int>(b.y1)-ts.height-4);
        cv::rectangle(img,{lx,ly},{lx+ts.width+4,ly+ts.height+4},col,cv::FILLED);
        cv::putText(img,label,{lx+2,ly+ts.height+1},
                    cv::FONT_HERSHEY_SIMPLEX,fontScale,{0,0,0},fontThick,cv::LINE_AA);
    }
}

// ─────────────────────────── PROCESS FRAME ─────────────────────────────────

cv::Mat ProviderSensor::processFrame(const cv::Mat& frame, int outWidth)
{
    using clk = std::chrono::steady_clock;
    using ms  = std::chrono::duration<double, std::milli>;
    auto T = [&](clk::time_point a, clk::time_point b){ return ms(b-a).count(); };

    // 1. Letterbox
    auto t0 = clk::now();
    LbInfo lb;
    cv::Mat padded=letterbox(frame, m_yolo->imgSize(), lb);
    auto t1 = clk::now();

    // 2. YOLO detect
    std::vector<Box> boxes_pad=m_yolo->detect(padded);
    auto t2 = clk::now();
    // {
    //     const auto& yt = m_yolo->timing();
    //     m_logger.LogInfo() << "[YOLO] pre=" << yt.pre
    //                        << "ms  infer=" << yt.infer
    //                        << "ms  post=" << yt.post << "ms";
    // }

    // 3. Inverse-map + box selection
    std::vector<Box> boxes_orig;
    boxes_orig.reserve(boxes_pad.size());
    for (const auto& b : boxes_pad) boxes_orig.push_back(inverseMap(b,lb));

    Box   best[3]; float best_conf[3]={-1,-1,-1}; bool has[3]={false,false,false};
    std::vector<Box> timers;
    for (const auto& b : boxes_orig){
        if (b.cls==CLS_TIMER){timers.push_back(b);continue;}
        for (int s=0;s<3;++s)
            if (b.cls==DIR_CLS[s]&&(!has[s]||b.conf>best_conf[s]))
                {best[s]=b;best_conf[s]=b.conf;has[s]=true;break;}
    }
    auto t3 = clk::now();

    // 4. Parallel direction threads: HSV color + OCR
    std::vector<DirResult> tasks;
    for (int s=0;s<3;++s){
        if (!has[s]) continue;
        DirResult r; r.name=DIR_NAMES[s]; r.dir_orig=best[s];
        tasks.push_back(std::move(r));
    }
    {
        std::vector<std::thread> threads;
        threads.reserve(tasks.size());
        for (auto& task : tasks)
            threads.emplace_back(dirWorker,std::ref(task),
                                 std::cref(frame),std::cref(timers),m_timerModel.get());
        for (auto& th : threads) th.join();
    }
    auto t4 = clk::now();

    // 5. Create display canvas (optionally downscaled) then draw boxes
    int dispW = (outWidth > 0 && outWidth < frame.cols) ? outWidth : frame.cols;
    int dispH = frame.rows * dispW / frame.cols;
    float sx = static_cast<float>(dispW) / frame.cols;
    float sy = static_cast<float>(dispH) / frame.rows;

    cv::Mat result;
    if (dispW == frame.cols) result = frame.clone();
    else cv::resize(frame, result, {dispW, dispH}, 0, 0, cv::INTER_LINEAR);

    // Scale helper: map a full-res Box to display-res Box
    auto scaleBox = [&](const Box& b) -> Box {
        return {b.x1*sx, b.y1*sy, b.x2*sx, b.y2*sy, b.conf, b.cls};
    };

    for (const auto& r : tasks){
        cv::Scalar light_col=colorOf(r.color);
        char dlbl[64];
        std::snprintf(dlbl,sizeof(dlbl),"%s|%s|%.2f",r.name,r.color.c_str(),r.dir_orig.conf);
        drawBox(result,scaleBox(r.dir_orig),light_col,dlbl);
        m_logger.LogInfo()<<"Direction="<<r.name<<" color="<<r.color.c_str()<<" conf="<<r.dir_orig.conf;
        if (r.has_timer){
            char tlbl[32];
            std::snprintf(tlbl,sizeof(tlbl),"timer:%s",r.timer_val.c_str());
            drawBox(result,scaleBox(r.timer_orig),{0,0,200},tlbl);
            m_logger.LogInfo()<<"  timer="<<r.timer_val.c_str();
        }
    }
    for (const auto& t : timers) drawBox(result,scaleBox(t),{200,200,0},"");
    auto t5 = clk::now();

    bool has_red_light = false;
    for (const auto& r : tasks) {
        if (r.color == "red") {
            has_red_light = true;
        }
    }
    m_isRedLight.store(has_red_light);

    // m_logger.LogInfo() << "[TIME] letterbox=" << T(t0,t1)
    //                    << "ms  detect=" << T(t1,t2)
    //                    << "ms  map+sel=" << T(t2,t3)
    //                    << "ms  ocr_threads=" << T(t3,t4)
    //                    << "ms  annotate=" << T(t4,t5) << "ms";
    return result;
}

// ─────────────────────────── GPS UART HELPERS ──────────────────────────────

double ProviderSensor::nmeaToDeg(const std::string& field, char hemi)
{
    if (field.empty()) return 0.0;
    double raw=std::stod(field);
    int    deg=static_cast<int>(raw/100);
    double min=raw-deg*100.0;
    double result=deg+min/60.0;
    if (hemi=='S'||hemi=='W') result=-result;
    return result;
}

std::vector<std::string> ProviderSensor::splitNmea(const std::string& line)
{
    std::vector<std::string> fields;
    std::istringstream ss(line);
    std::string tok;
    while (std::getline(ss,tok,',')) fields.push_back(tok);
    return fields;
}

bool ProviderSensor::parseNmeaFix(const std::string& raw, Coordinates& out)
{
    if (raw.size()<6) return false;
    std::string s=(raw[0]=='$')?raw.substr(1):raw;
    auto star=s.rfind('*');
    if (star!=std::string::npos) s=s.substr(0,star);
    auto f=splitNmea(s);
    if (f.empty()) return false;

    if ((f[0]=="GNGLL"||f[0]=="GPGLL")&&f.size()>=7){
        if (f[6]!="A"||f[1].empty()||f[3].empty()) return false;
        out.latitude =nmeaToDeg(f[1],f[2].empty()?'N':f[2][0]);
        out.longitude=nmeaToDeg(f[3],f[4].empty()?'E':f[4][0]);
        return true;
    }
    if ((f[0]=="GNRMC"||f[0]=="GPRMC")&&f.size()>=7){
        if (f[2]!="A"||f[3].empty()||f[5].empty()) return false;
        out.latitude =nmeaToDeg(f[3],f[4].empty()?'N':f[4][0]);
        out.longitude=nmeaToDeg(f[5],f[6].empty()?'E':f[6][0]);
        return true;
    }
    if ((f[0]=="GPGGA"||f[0]=="GNGGA")&&f.size()>=7){
        if (f[6]=="0"||f[2].empty()||f[4].empty()) return false;
        out.latitude =nmeaToDeg(f[2],f[3].empty()?'N':f[3][0]);
        out.longitude=nmeaToDeg(f[4],f[5].empty()?'E':f[5][0]);
        return true;
    }
    return false;
}

int ProviderSensor::openGPSUART()
{
    int fd=open(GPS_UART_DEV,O_RDONLY|O_NOCTTY);
    if (fd<0) return -1;
    struct termios tty{};
    if (tcgetattr(fd,&tty)!=0){close(fd);return -1;}
    cfmakeraw(&tty);
    cfsetispeed(&tty,GPS_BAUD);
    tty.c_cc[VMIN]=0; tty.c_cc[VTIME]=0;
    if (tcsetattr(fd,TCSANOW,&tty)!=0){close(fd);return -1;}
    return fd;
}

bool ProviderSensor::readNextGPSFix(int fd, Coordinates& fix)
{
    char buf[GPS_BUF_SIZE];
    char line[GPS_BUF_SIZE];
    int  linePos=0;
    while (true){
        fd_set rfds; FD_ZERO(&rfds); FD_SET(fd,&rfds);
        struct timeval tv{GPS_TIMEOUT_S,0};
        int ret=select(fd+1,&rfds,nullptr,nullptr,&tv);
        if (ret<0){if(errno==EINTR)continue;return false;}
        if (ret==0) return false;
        ssize_t n=read(fd,buf,sizeof(buf));
        if (n==0) return false;
        if (n<0){if(errno==EINTR)continue;return false;}
        for (ssize_t i=0;i<n;++i){
            char c=buf[i];
            if (c=='\n'){
                line[linePos]='\0'; linePos=0;
                if (parseNmeaFix(std::string(line),fix)) return true;
            } else if (c!='\r'&&linePos<GPS_BUF_SIZE-1){
                line[linePos++]=c;
            }
        }
    }
}

// ─────────────────────────── INIT MODELS ───────────────────────────────────

bool ProviderSensor::InitializeModels()
{
    m_logger.LogInfo() << "Loading YOLO26n e2e (int8 → fp16 → fp32 fallback)...";
    try {
        m_yolo = std::make_unique<YoloDetector>();
    } catch (const std::exception& e) {
        m_logger.LogError() << "YOLO load failed: " << e.what();
        return false;
    }
    m_logger.LogInfo() << "YOLO ready. imgSize=" << m_yolo->imgSize();

    m_logger.LogInfo() << "Loading timer OCR: " << TIMER_MODEL;
    m_timerModel = tflite::FlatBufferModel::BuildFromFile(TIMER_MODEL);
    if (!m_timerModel){
        m_logger.LogError() << "Cannot load timer model: " << TIMER_MODEL;
        return false;
    }
    return true;
}

// ─────────────────────────── LIFECYCLE ─────────────────────────────────────

bool ProviderSensor::Initialize()
{
    m_logger.LogVerbose() << "ProviderSensor::Initialize";
    m_PPort0=std::make_unique<providersensor::aa::port::PPort0>();
    if (!InitializeModels()){
        m_logger.LogError() << "Failed to initialize models";
        return false;
    }
    return true;
}

void ProviderSensor::Start()
{
    m_logger.LogVerbose() << "ProviderSensor::Start";
    m_PPort0->Start();
}

void ProviderSensor::Terminate()
{
    m_logger.LogVerbose() << "ProviderSensor::Terminate";
    m_running=false;
    m_PPort0->Terminate();
}

void ProviderSensor::Run()
{
    m_logger.LogVerbose() << "ProviderSensor::Run";
    m_running=true;

    // Pin this process to cores 0,1,2 — leaves core 3 exclusively for show_cam.py
    {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(0, &cpuset);
        CPU_SET(1, &cpuset);
        CPU_SET(2, &cpuset);
        if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == 0)
            m_logger.LogInfo() << "[SYS] Affinity set: cores 0,1,2 (core 3 reserved for display)";
        else
            m_logger.LogWarn() << "[SYS] sched_setaffinity failed — running on all cores";
    }

    // ── GPS thread: reads UART, updates seqlock snapshot ─────────────────
    m_workers.Async([this]{
        std::vector<TrafficNode> nodes;
        try { nodes=parseGeoJSON(GEOJSON_FILE_NAME); }
        catch (const std::exception& e){ m_logger.LogError()<<"[GPS] GeoJSON: "<<e.what(); return; }
        if (nodes.empty()){ m_logger.LogError()<<"[GPS] No traffic nodes."; return; }
        KDTree tree(std::move(nodes));
        m_logger.LogInfo()<<"[GPS] KD-tree ready.";

        int gpsFd=-1;
        int openRetries=0;
        while (m_running&&gpsFd<0){
            gpsFd=openGPSUART();
            if (gpsFd<0){
                if (++openRetries>=GPS_MAX_RETRIES){
                    m_logger.LogWarn()<<"[GPS] UART unavailable after "<<GPS_MAX_RETRIES
                                      <<" retries – using hardcoded location ("
                                      <<GPS_FALLBACK_LAT<<","<<GPS_FALLBACK_LON<<")";
                    try {
                        Coordinates fixed{GPS_FALLBACK_LAT, GPS_FALLBACK_LON};
                        TrafficNode nn=tree.nearest(fixed);
                        double dist=haversine(fixed,nn.coords);
                        publishGpsSnapshot(static_cast<std::int64_t>(nn.id),
                                           static_cast<std::int32_t>(std::lround(dist)),
                                           dist<=static_cast<double>(RANGE_SIZE)?1u:0u);
                        m_logger.LogInfo()<<"[GPS] Fixed location: nearest id="
                                          <<static_cast<int64_t>(nn.id)<<" dist="<<dist<<"m";
                    } catch (const std::exception& e){
                        m_logger.LogError()<<"[GPS] KD-tree (fixed): "<<e.what();
                    }
                    while (m_running)
                        std::this_thread::sleep_for(std::chrono::seconds(10));
                    return;
                }
                m_logger.LogWarn()<<"[GPS] Cannot open "<<GPS_UART_DEV<<" – retrying...";
                std::this_thread::sleep_for(std::chrono::seconds(GPS_RETRY_DELAY));
            }
        }
        if (gpsFd<0) return;
        m_logger.LogInfo()<<"[GPS] UART open.";

        while (m_running){
            Coordinates gpsCoord{};
            if (!readNextGPSFix(gpsFd,gpsCoord)){
                close(gpsFd); gpsFd=-1;
                m_logger.LogWarn()<<"[GPS] Connection lost – reconnecting...";
                while (m_running&&gpsFd<0){
                    std::this_thread::sleep_for(std::chrono::seconds(GPS_RETRY_DELAY));
                    gpsFd=openGPSUART();
                }
                if (gpsFd>=0) m_logger.LogInfo()<<"[GPS] Reconnected.";
                continue;
            }
            try {
                TrafficNode nn=tree.nearest(gpsCoord);
                double dist=haversine(gpsCoord,nn.coords);
                const std::int32_t distM=static_cast<std::int32_t>(std::lround(dist));
                const std::uint8_t inR=(dist<=static_cast<double>(RANGE_SIZE))?1u:0u;
                publishGpsSnapshot(static_cast<std::int64_t>(nn.id), distM, inR);

                m_logger.LogInfo()<<"[GPS] lat="<<gpsCoord.latitude
                                  <<" lon="<<gpsCoord.longitude
                                  <<" nearest id="<<static_cast<int64_t>(nn.id)
                                  <<" dist="<<dist<<"m";
                if (inR)
                    m_logger.LogWarn()<<"[GPS] ALERT: within "<<RANGE_SIZE
                                      <<"m of node id="<<static_cast<int64_t>(nn.id)
                                      <<" (dist="<<dist<<"m)";
            } catch (const std::exception& e){
                m_logger.LogError()<<"[GPS] KD-tree: "<<e.what();
            }
        }
        if (gpsFd>=0) close(gpsFd);
    });

    // ── Camera / vision thread ────────────────────────────────────────────
    m_workers.Async([this]{
        // Try live camera first; fall back to test video if unavailable
        cv::VideoCapture cap(0);
        cap.set(cv::CAP_PROP_FRAME_WIDTH,  640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

        bool usingVideo=false;
        if (!cap.isOpened()){
            m_logger.LogWarn()<<"[CAM] Camera unavailable – opening fallback: "<<VIDEO_FALLBACK;
            // Explicitly request FFMPEG to avoid OpenCV falling back to the
            // image-sequence backend (CAP_IMAGES) which cannot handle .MOV files
            cap.open(VIDEO_FALLBACK, cv::CAP_FFMPEG);
            if (!cap.isOpened())
                cap.open(VIDEO_FALLBACK, cv::CAP_GSTREAMER);
            if (!cap.isOpened())
                cap.open(VIDEO_FALLBACK);   // last-resort: any backend
            usingVideo=true;
        }
        if (!cap.isOpened()){
            m_logger.LogError()<<"[CAM] Cannot open camera or "<<VIDEO_FALLBACK
                               <<" (ensure ffmpeg/gstreamer support in OpenCV)";
            m_running = false;   // unblock GPS thread's while(m_running) sleep loop
            return;
        }
        m_logger.LogInfo()<<(usingVideo?"[CAM] Using video file":"[CAM] Camera opened");

        cv::Mat frame;
        while (m_running){
            auto t_start=std::chrono::high_resolution_clock::now();

            bool ret=cap.read(frame);
            if (!ret||frame.empty()){
                if (usingVideo){
                    cap.set(cv::CAP_PROP_POS_FRAMES,0);   // loop video
                    ret=cap.read(frame);
                }
                if (!ret||frame.empty()){
                    m_logger.LogError()<<"[CAM] Failed to grab frame";
                    std::this_thread::sleep_for(std::chrono::milliseconds(33));
                    continue;
                }
            }

            using clk = std::chrono::high_resolution_clock;
            using fms = std::chrono::duration<double, std::milli>;

            // Run pipeline — returns 320-wide annotated image directly
            auto ta = clk::now();
            cv::Mat result = processFrame(frame, 0);
            auto tb = clk::now();

            // Encode — result is already 320px wide, no extra resize needed
            std::vector<uchar> jpegBuf;
            cv::imencode(".jpg", result, jpegBuf, {cv::IMWRITE_JPEG_QUALITY, 95});
            auto tc = clk::now();

            // Read latest GPS snapshot (lock-free seqlock)
            GpsSnapshot snap{};
            tryCopyGpsSnapshot(snap);

            // Pack GPS metadata + JPEG into a single wire buffer
            GpsJpegWireHeader hdr{};
            hdr.magic    = kGpsJpegMagic;
            hdr.version  = 1;
            hdr.in_range = snap.inRange;
            hdr.reserved[0] = m_isRedLight.load() ? 1u : 0u;
            hdr.reserved[1] = 0;
            hdr.node_id  = snap.nodeId;
            hdr.dist_m   = snap.distM;
            hdr.jpeg_len = static_cast<std::uint32_t>(jpegBuf.size());

            std::vector<int8_t> wire(sizeof(GpsJpegWireHeader)+jpegBuf.size());
            std::memcpy(wire.data(), &hdr, sizeof(GpsJpegWireHeader));
            if (!jpegBuf.empty())
                std::memcpy(wire.data()+sizeof(GpsJpegWireHeader), jpegBuf.data(), jpegBuf.size());
            auto td = clk::now();

            sensorCam::skeleton::events::imageDataEvent::SampleType autosar_buf(wire.begin(),wire.end());
            m_PPort0->WriteDataimageDataEvent(autosar_buf);
            m_PPort0->SendEventimageDataEventTriggered();
            auto te = clk::now();

            double total_ms = fms(te - t_start).count();
            // m_logger.LogInfo() << "[CAM] cap=" << fms(ta-t_start).count()
            //                    << "ms  pipeline=" << fms(tb-ta).count()
            //                    << "ms  imencode=" << fms(tc-tb).count()
            //                    << "ms  pack=" << fms(td-tc).count()
            //                    << "ms  dds=" << fms(te-td).count()
            //                    << "ms  TOTAL=" << total_ms
            //                    << "ms  jpeg=" << jpegBuf.size() << "B";
        }
    });

    m_workers.Wait();
}

} // namespace aa
} // namespace providersensor
