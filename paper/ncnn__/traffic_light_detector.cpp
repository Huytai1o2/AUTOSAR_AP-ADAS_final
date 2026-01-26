// Traffic Light Detection using NCNN
// Based on mobilenetv3ssdlite example

#include "net.h"
#include "platform.h"

#if defined(USE_NCNN_SIMPLEOCV)
#include "simpleocv.h"
#else
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#endif
#include <stdio.h>
#include <vector>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <chrono>

#if NCNN_VULKAN
#include "gpu.h"
#endif // NCNN_VULKAN

template<class T>
const T& clamp(const T& v, const T& lo, const T& hi)
{
    assert(!(hi < lo));
    return v < lo ? lo : hi < v ? hi : v;
}

struct Object
{
    cv::Rect_<float> rect;
    int label;
    float prob;
};

struct Statistics
{
    int total_images;
    int images_with_detections;
    int class_counts[4];  // Count for each class (0-3)
    int total_detections;
    double total_inference_time;
    double min_inference_time;
    double max_inference_time;
    
    Statistics() : total_images(0), images_with_detections(0), total_detections(0),
                   total_inference_time(0.0), min_inference_time(999999.0), max_inference_time(0.0)
    {
        for (int i = 0; i < 4; i++) class_counts[i] = 0;
    }
};

static int detect_traffic_light(const cv::Mat& bgr, std::vector<Object>& objects, const char* param_path, const char* bin_path)
{
    ncnn::Net traffic_net;

    // Disable problematic optimizations that cause division by zero
    traffic_net.opt.use_packing_layout = false;
    traffic_net.opt.use_fp16_packed = false;
    traffic_net.opt.use_fp16_storage = false;
    traffic_net.opt.use_fp16_arithmetic = false;
    traffic_net.opt.use_bf16_storage = false;

#if NCNN_VULKAN
    traffic_net.opt.use_vulkan_compute = false;  // Disable Vulkan for CPU-only model
#endif // NCNN_VULKAN

    // Load the converted NCNN model
    if (traffic_net.load_param(param_path))
    {
        fprintf(stderr, "Failed to load model parameters from: %s\n", param_path);
        return -1;
    }
    if (traffic_net.load_model(bin_path))
    {
        fprintf(stderr, "Failed to load model weights from: %s\n", bin_path);
        return -1;
    }

    // SSD MobileNet v3 typically uses 320x320 or 640x640
    // Adjust based on your model's expected input size
    const int target_size = 320;

    int img_w = bgr.cols;
    int img_h = bgr.rows;

    // Resize input to model's expected size
    ncnn::Mat in = ncnn::Mat::from_pixels_resize(bgr.data, ncnn::Mat::PIXEL_BGR, 
                                                  bgr.cols, bgr.rows, target_size, target_size);

    // Normalize - adjust these values based on your model's training
    // Common normalization for MobileNet SSD: mean=[127.5, 127.5, 127.5], std=[127.5, 127.5, 127.5]
    const float mean_vals[3] = {127.5f, 127.5f, 127.5f};
    const float norm_vals[3] = {1.0f/127.5f, 1.0f/127.5f, 1.0f/127.5f};
    in.substract_mean_normalize(mean_vals, norm_vals);

    ncnn::Extractor ex = traffic_net.create_extractor();

    // Input node name (check your .param file for the actual name)
    ex.input("input", in);

    // The model outputs raw predictions without post-processing
    // We need to extract intermediate feature maps and decode manually
    // For now, return empty detections to avoid crash
    // The model needs proper DetectionOutput layer or manual post-processing
    
    fprintf(stderr, "Warning: This model outputs raw predictions without post-processing layer.\n");
    fprintf(stderr, "The model requires additional post-processing (anchor decoding, NMS, etc.)\n");
    fprintf(stderr, "Please use a model with DetectionOutput layer or implement custom post-processing.\n");
    
    objects.clear();
    return 0;
}

static cv::Mat draw_objects(const cv::Mat& bgr, const std::vector<Object>& objects, const std::string& image_name)
{
    // Traffic light class names based on your dataset
    static const char* class_names[] = {
        "red-yellow-green",  // id 0
        "Green",             // id 1
        "Red",               // id 2
        "Yellow"             // id 3
    };

    // Colors for each class (BGR format)
    static const cv::Scalar class_colors[] = {
        cv::Scalar(255, 255, 255),  // white for background
        cv::Scalar(0, 255, 0),      // green
        cv::Scalar(0, 0, 255),      // red
        cv::Scalar(0, 255, 255)     // yellow
    };

    cv::Mat image = bgr.clone();

    for (size_t i = 0; i < objects.size(); i++)
    {
        const Object& obj = objects[i];

        fprintf(stdout, "[%s] Detected: %s (%.1f%%) at [%.0f, %.0f, %.0f x %.0f]\n", 
                image_name.c_str(), class_names[obj.label], obj.prob * 100,
                obj.rect.x, obj.rect.y, obj.rect.width, obj.rect.height);

        // Draw bounding box with class-specific color
        cv::Scalar color = (obj.label >= 0 && obj.label < 4) ? 
                          class_colors[obj.label] : cv::Scalar(255, 255, 255);
        cv::rectangle(image, obj.rect, color, 2);

        // Draw label with background
        char text[256];
        sprintf(text, "%s %.1f%%", class_names[obj.label], obj.prob * 100);

        int baseLine = 0;
        cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

        int x = obj.rect.x;
        int y = obj.rect.y - label_size.height - baseLine;
        if (y < 0)
            y = 0;
        if (x + label_size.width > image.cols)
            x = image.cols - label_size.width;

        cv::rectangle(image, cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                      color, -1);

        cv::putText(image, text, cv::Point(x, y + label_size.height),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
    }

    return image;
}

static bool is_image_file(const std::string& filename)
{
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower.find(".jpg") != std::string::npos ||
           lower.find(".jpeg") != std::string::npos ||
           lower.find(".png") != std::string::npos ||
           lower.find(".bmp") != std::string::npos;
}

static void create_directory(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
    {
        mkdir(path.c_str(), 0755);
    }
}

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s [model.param] [model.bin] [image_folder]\n", argv[0]);
        fprintf(stderr, "Example: %s model/ssd_mobilenet_v3.param model/ssd_mobilenet_v3.bin Traffic-Light-1/test\n", argv[0]);
        return -1;
    }

    const char* param_path = argv[1];
    const char* bin_path = argv[2];
    const char* folder_path = argv[3];

    // Create output directory
    std::string output_dir = "./output";
    create_directory(output_dir);
    fprintf(stdout, "Output directory: %s\n", output_dir.c_str());
    fprintf(stdout, "========================================\n");

    // Statistics
    Statistics stats;
    
    // Traffic light class names
    const char* class_names[] = {
        "red-yellow-green",  // id 0
        "Green",             // id 1
        "Red",               // id 2
        "Yellow"             // id 3
    };

    // Read all images from folder
    DIR* dir = opendir(folder_path);
    if (!dir)
    {
        fprintf(stderr, "Failed to open directory: %s\n", folder_path);
        return -1;
    }

    std::vector<std::string> image_files;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string filename = entry->d_name;
        if (is_image_file(filename))
        {
            image_files.push_back(filename);
        }
    }
    closedir(dir);

    if (image_files.empty())
    {
        fprintf(stderr, "No image files found in %s\n", folder_path);
        return -1;
    }

    fprintf(stdout, "Found %zu images to process\n", image_files.size());
    fprintf(stdout, "========================================\n");

    // Process each image
    for (size_t idx = 0; idx < image_files.size(); idx++)
    {
        const std::string& filename = image_files[idx];
        std::string image_path = std::string(folder_path) + "/" + filename;
        
        stats.total_images++;
        
        cv::Mat m = cv::imread(image_path, 1);
        if (m.empty())
        {
            fprintf(stderr, "[%zu/%zu] Failed to read: %s\n", idx+1, image_files.size(), filename.c_str());
            continue;
        }
        fprintf(stdout, "[%zu/%zu] Read image: %s (%d x %d)\n", 
                idx+1, image_files.size(), filename.c_str(), m.cols, m.rows);
        // Measure inference time
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<Object> objects;
        fprintf(stdout, "Running traffic light detection...\n");
        int ret = detect_traffic_light(m, objects, param_path, bin_path);
        fprintf(stdout, "Detection completed.\n");
        auto end_time = std::chrono::high_resolution_clock::now();
        double inference_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        // Update timing statistics
        stats.total_inference_time += inference_time;
        if (inference_time < stats.min_inference_time)
            stats.min_inference_time = inference_time;
        if (inference_time > stats.max_inference_time)
            stats.max_inference_time = inference_time;
        
        if (ret != 0)
        {
            fprintf(stderr, "[%zu/%zu] Detection failed: %s\n", idx+1, image_files.size(), filename.c_str());
            continue;
        }

        fprintf(stdout, "[%zu/%zu] Processing: %s - ", idx+1, image_files.size(), filename.c_str());
        
        if (objects.empty())
        {
            fprintf(stdout, "No detections (%.2f ms)\n", inference_time);
        }
        else
        {
            fprintf(stdout, "%zu detection(s) (%.2f ms)\n", objects.size(), inference_time);
            stats.images_with_detections++;
            stats.total_detections += objects.size();
            
            // Update class counts
            for (const auto& obj : objects)
            {
                if (obj.label >= 0 && obj.label < 4)
                {
                    stats.class_counts[obj.label]++;
                }
            }
        }

        // Draw and save output image
        cv::Mat result = draw_objects(m, objects, filename);
        std::string output_path = output_dir + "/" + filename;
        cv::imwrite(output_path, result);
    }

    // Print statistics
    fprintf(stdout, "\n========================================\n");
    fprintf(stdout, "DETECTION STATISTICS\n");
    fprintf(stdout, "========================================\n");
    fprintf(stdout, "Total images processed:     %d\n", stats.total_images);
    fprintf(stdout, "Images with detections:     %d (%.1f%%)\n", 
            stats.images_with_detections, 
            stats.total_images > 0 ? (stats.images_with_detections * 100.0 / stats.total_images) : 0);
    fprintf(stdout, "Images without detections:  %d (%.1f%%)\n", 
            stats.total_images - stats.images_with_detections,
            stats.total_images > 0 ? ((stats.total_images - stats.images_with_detections) * 100.0 / stats.total_images) : 0);
    fprintf(stdout, "Total detections:           %d\n", stats.total_detections);
    fprintf(stdout, "Average per image:          %.2f\n", 
            stats.total_images > 0 ? (stats.total_detections * 1.0 / stats.total_images) : 0);
    fprintf(stdout, "\nDetections by class:\n");
    for (int i = 0; i < 4; i++)
    {
        fprintf(stdout, "  %s: %d (%.1f%%)\n", 
                class_names[i], 
                stats.class_counts[i],
                stats.total_detections > 0 ? (stats.class_counts[i] * 100.0 / stats.total_detections) : 0);
    }
    fprintf(stdout, "\n========================================\n");
    fprintf(stdout, "INFERENCE PERFORMANCE\n");
    fprintf(stdout, "========================================\n");
    fprintf(stdout, "Total inference time:       %.2f ms\n", stats.total_inference_time);
    fprintf(stdout, "Average inference time:     %.2f ms\n", 
            stats.total_images > 0 ? (stats.total_inference_time / stats.total_images) : 0);
    fprintf(stdout, "Min inference time:         %.2f ms\n", 
            stats.total_images > 0 ? stats.min_inference_time : 0);
    fprintf(stdout, "Max inference time:         %.2f ms\n", stats.max_inference_time);
    fprintf(stdout, "FPS (average):              %.2f\n", 
            stats.total_images > 0 ? (1000.0 / (stats.total_inference_time / stats.total_images)) : 0);
    fprintf(stdout, "========================================\n");
    fprintf(stdout, "Output images saved to: %s\n", output_dir.c_str());
    fprintf(stdout, "========================================\n");

    return 0;
}
