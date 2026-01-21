// ============================================
// TFLite YOLOv11 (cx,cy,w,h + 6 class probs)
// Output tensor: [1, 10, N]
// ============================================
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <numeric>
#include <array>
#include <chrono>                  // <-- thêm
#include <opencv2/opencv.hpp>

#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/optional_debug_tools.h"
#include "tensorflow/lite/c/common.h"

struct Det {
    cv::Rect rect;
    int cls;
    float score;
};

static float IoU(const cv::Rect& a, const cv::Rect& b){
    int x1 = std::max(a.x, b.x);
    int y1 = std::max(a.y, b.y);
    int x2 = std::min(a.x + a.width,  b.x + b.width);
    int y2 = std::min(a.y + a.height, b.y + b.height);
    int w = std::max(0, x2 - x1);
    int h = std::max(0, y2 - y1);
    float inter = float(w) * float(h);
    float uni   = float(a.area() + b.area()) - inter;
    return uni > 0 ? inter / uni : 0.0f;
}

static std::vector<Det> nms(const std::vector<Det>& dets, float iou_thr){
    if (dets.empty()) return {};
    std::vector<int> idx(dets.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(),
              [&](int a, int b){ return dets[a].score > dets[b].score; });

    std::vector<Det> keep;
    std::vector<char> suppressed(dets.size(), 0);
    for (size_t _i = 0; _i < idx.size(); ++_i){
        int i = idx[_i];
        if (suppressed[i]) continue;
        keep.push_back(dets[i]);
        for (size_t _j = _i + 1; _j < idx.size(); ++_j){
            int j = idx[_j];
            if (suppressed[j]) continue;
            if (dets[i].cls == dets[j].cls &&
                IoU(dets[i].rect, dets[j].rect) > iou_thr)
            {
                suppressed[j] = 1;
            }
        }
    }
    return keep;
}

int main(){
    // ----------------- Load model -----------------
    auto model = tflite::FlatBufferModel::BuildFromFile(
        "../bestYoloMobilenet_saved_model/bestYoloMobilenet_float16.tflite");
    if (!model){
        std::cerr << "Failed to load model\n";
        return 1;
    }

    tflite::ops::builtin::BuiltinOpResolver resolver;
    std::unique_ptr<tflite::Interpreter> interpreter;
    if (tflite::InterpreterBuilder(*model, resolver)(&interpreter) != kTfLiteOk ||
        !interpreter)
    {
        std::cerr << "Failed to build interpreter\n";
        return 1;
    }

    interpreter->SetNumThreads(4);
    if (interpreter->AllocateTensors() != kTfLiteOk){
        std::cerr << "AllocateTensors failed\n";
        return 1;
    }

    // ----------------- Input info -----------------
    int in_idx = interpreter->inputs()[0];
    TfLiteTensor* in_t = interpreter->tensor(in_idx);
    if (in_t->type != kTfLiteFloat32 || in_t->dims->size != 4){
        std::cerr << "Expect NHWC float32 input\n";
        return 1;
    }
    int ib = in_t->dims->data[0];
    int ih = in_t->dims->data[1];
    int iw = in_t->dims->data[2];
    int ic = in_t->dims->data[3];
    std::cout << "Input tensor shape: ["<<ib<<","<<ih<<","<<iw<<","<<ic<<"]\n";

    // =====================================================
    // Preprocessing timer START
    // =====================================================
    auto t_pre_start = std::chrono::high_resolution_clock::now();

    // ----------------- Read & preprocess image -----------------
    cv::Mat img = cv::imread("../yellowLights.jpg");
    if (img.empty()){
        std::cerr << "Read image failed\n";
        return 1;
    }
    int H = img.rows, W = img.cols;

    cv::Mat resized;
    cv::resize(img, resized, cv::Size(iw, ih));

    // BGR -> RGB như Ultralytics
    cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);

    cv::Mat f32;
    resized.convertTo(f32, CV_32FC3, 1.f/255.f); // [0,1]

    std::memcpy(interpreter->typed_input_tensor<float>(0),
                f32.data, f32.total()*f32.elemSize());
    std::cout << "Input image preprocessed and copied to tensor.\n";

    auto t_pre_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> t_pre_ms = t_pre_end - t_pre_start;
    std::cout << "Preprocess time: " << t_pre_ms.count() << " ms\n";

    // =====================================================
    // Inference timer
    // =====================================================
    auto t_inf_start = std::chrono::high_resolution_clock::now();
    if (interpreter->Invoke() != kTfLiteOk){
        std::cerr << "Invoke failed\n";
        return 1;
    }
    auto t_inf_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> t_inf_ms = t_inf_end - t_inf_start;
    std::cout << "Inference time: " << t_inf_ms.count() << " ms\n";

    // =====================================================
    // Post-processing timer START
    // =====================================================
    auto t_post_start = std::chrono::high_resolution_clock::now();

    // ----------------- Output info -----------------
    const auto& outs = interpreter->outputs();
    if (outs.size() != 1){
        std::cerr << "Expect 1 output, got " << outs.size() << "\n";
        return 1;
    }
    int out_idx = outs[0];
    const TfLiteTensor* ot = interpreter->tensor(out_idx);
    if (ot->type != kTfLiteFloat32 ||
        ot->dims->size != 3 ||
        ot->dims->data[0] != 1 ||
        ot->dims->data[1] != 10)
    {
        std::cerr << "Unexpected output shape; need [1,10,N]\n";
        return 1;
    }

    const int C = ot->dims->data[1]; // 10 = 4 box + 6 classes
    const int N = ot->dims->data[2];
    std::cout << "Output shape: [1,"<<C<<","<<N<<"]\n";

    const float* out = interpreter->typed_tensor<float>(out_idx); // [1,C,N] -> [C,N]

    // ---- Giống ops.non_max_suppression ----
    const float conf_thr = 0.25f;   // giống YOLO
    const float iou_thr  = 0.45f;
    const int   nc       = C - 4;   // 6 lớp

    // 1) scale box normalized -> pixel trong không gian input (ih, iw)
    std::vector<std::array<float,10>> pred(N); // [N, C]
    for (int c = 0; c < C; ++c){
        const float* src = out + c * N;   // [C,N]
        for (int i = 0; i < N; ++i){
            float v = src[i];
            if (c == 0 || c == 2) v *= (float)iw; // theo width
            if (c == 1 || c == 3) v *= (float)ih; // theo height
            pred[i][c] = v;
        }
    }

    // 2) conf = max class prob, filter > conf_thr
    std::vector<Det> proposals;
    proposals.reserve(N);

    for (int i = 0; i < N; ++i){
        const auto& p = pred[i];

        // best class & conf
        int   best_cls = -1;
        float best_p   = 0.f;
        for (int c = 0; c < nc; ++c){ // 6 lớp
            float pc = p[4 + c];      // channels 4..9
            if (pc > best_p){
                best_p   = pc;
                best_cls = c;
            }
        }
        if (best_p < conf_thr) continue;

        // 3) cx,cy,w,h (theo input 320x320) -> x1,y1,x2,y2
        float cx = p[0];
        float cy = p[1];
        float bw = p[2];
        float bh = p[3];

        float x1 = cx - bw * 0.5f;
        float y1 = cy - bh * 0.5f;
        float x2 = cx + bw * 0.5f;
        float y2 = cy + bh * 0.5f;

        // 4) scale box từ kích thước input (ih,iw) -> kích thước ảnh gốc (H,W)
        float gain_x = (float)W / (float)iw;
        float gain_y = (float)H / (float)ih;
        x1 *= gain_x; x2 *= gain_x;
        y1 *= gain_y; y2 *= gain_y;

        // clamp
        x1 = std::min(std::max(x1, 0.f), float(W-1));
        y1 = std::min(std::max(y1, 0.f), float(H-1));
        x2 = std::min(std::max(x2, 0.f), float(W-1));
        y2 = std::min(std::max(y2, 0.f), float(H-1));

        int rw = int(std::max(0.f, x2 - x1));
        int rh = int(std::max(0.f, y2 - y1));
        if (rw <= 0 || rh <= 0) continue;

        proposals.push_back(Det{ cv::Rect((int)x1, (int)y1, rw, rh),
                                 best_cls, best_p });
    }

    std::cout << "Proposals after conf filter: " << proposals.size() << "\n";

    // 5) NMS per class
    std::vector<Det> final_dets = nms(proposals, iou_thr);
    std::cout << "Detections after NMS: " << final_dets.size() << "\n";

    // ----------------- Draw -----------------
    static const char* names[6] = {
        "green-lights","red-lights","traffic-lights",
        "turn-left","turn-right","yellow-lights"
    };

    for (const auto& d : final_dets){
        cv::rectangle(img, d.rect, cv::Scalar(0,255,0), 2);
        char text[128];
        std::snprintf(text, sizeof(text), "%s %.2f",
                      (d.cls>=0 && d.cls<6)?names[d.cls]:"cls?",
                      d.score);
        int base = 0;
        cv::Size ts = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX,
                                      0.6, 2, &base);
        int x1 = d.rect.x;
        int y1 = std::max(0, d.rect.y - ts.height - 6);
        cv::rectangle(img,
                      cv::Point(x1, y1),
                      cv::Point(x1 + ts.width + 6, y1 + ts.height + 6),
                      cv::Scalar(0,255,0), cv::FILLED);
        cv::putText(img, text,
                    cv::Point(x1 + 3, y1 + ts.height + 1),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6,
                    cv::Scalar(0,0,0), 2);
    }

    cv::imwrite("result.jpg", img);
    std::cout << "Saved annotated image: result.jpg\n";

    auto t_post_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> t_post_ms = t_post_end - t_post_start;
    std::cout << "Postprocess time (decode + NMS + draw + save): "
              << t_post_ms.count() << " ms\n";

    // Tổng nếu bạn muốn
    double total_ms = t_pre_ms.count() + t_inf_ms.count() + t_post_ms.count();
    std::cout << "Total pipeline time: " << total_ms << " ms\n";

    return 0;
}
