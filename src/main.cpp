#include <iostream>
#include <memory>
#include <random>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

// #define NANOVG_GL3_IMPLEMENTATION
// #include <GLFW/glfw3.h>
// #include "nanovg.h"
// #include "nanovg_gl.h"
// #include "nanovg_gl_utils.h"

#include "render_module/render_module.hpp" 



enum class PoseInteractionState {
    kInactive,
    kAwaitingStartClick,
    kAwaitingDragDirection,
    kComplete
};

struct Pose {
    bool active = false;
    float x = 0.0f;
    float y = 0.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    float theta = 0.0f;
};


bool HandlePoseInteractionStateMachine(Pose& pose,
                                       PoseInteractionState& state,
                                       const ImVec2& canvasPos,
                                       const ImVec2& canvasSize)
{
    bool interactionComplete = false;

    if (state != PoseInteractionState::kInactive) {

        ImVec2 cursorBackup = ImGui::GetCursorPos();
        ImGui::SetCursorScreenPos(canvasPos);
        ImGui::InvisibleButton("##PoseInteraction", canvasSize,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        ImGui::SetCursorPos(cursorBackup);

        bool hovering = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        ImVec2 delta = ImGui::GetIO().MouseDelta;

        switch (state) {
            case PoseInteractionState::kInactive:
                // Do nothing unless externally activated
                break;

            case PoseInteractionState::kAwaitingStartClick:
                if (hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    pose.x = mousePos.x - canvasPos.x;
                    pose.y = canvasSize.y - (mousePos.y - canvasPos.y);  // Flip Y
                    pose.dx = 0.0f;
                    pose.dy = 0.0f;
                    state = PoseInteractionState::kAwaitingDragDirection;
                }
                break;

            case PoseInteractionState::kAwaitingDragDirection:
                if (hovering && active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    pose.dx += delta.x;
                    pose.dy -= delta.y;  // Flip Y
                }

                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    state = PoseInteractionState::kComplete;
                    interactionComplete = true;
                    pose.active = true;  // Mark pose as active
                }
                break;

            case PoseInteractionState::kComplete:
                // Optional: reset or wait for external signal
                break;
        }
    }

    return interactionComplete;
}



int main() {

    // constexpr auto img_path = "../data/maze-1-10x10.png";
    constexpr auto img_path = "../data/maze-1-100x100.png";
    // constexpr auto img_path = "../data/maze-4-500x500.png";
    int width, height, channels;

    std::cout << "Loading image: " << img_path << "\n";

    // Custom deleter using stbi_image_free
    auto stbi_deleter = [](unsigned char* p) { stbi_image_free(p); };
    int required_channels = 1;

    std::unique_ptr<unsigned char, decltype(stbi_deleter)>
        data(stbi_load(img_path, &width, &height, &channels, required_channels), stbi_deleter);
    channels = required_channels;
    
    if (!data) {
        std::cerr << "Failed to load image\n";
        return 1;
    }

    std::cout << "Loaded image: " << width << "x" << height
              << " with " << channels << " channels.\n";


    /** Create ROS-style Occupancy Grid Map 
     * - int8[] data ... probability [0,100], unknown = -1
    */
    struct MapMetaData {
        int width;
        int height;
        float resolution; // meters per pixel
        float origin_x;  // origin in meters
        float origin_y;  // origin in meters
    };
    MapMetaData map_metadata {
        .width = width,
        .height = height,
        .resolution = 0.1f, // 0.1 m per cell
        .origin_x = 0.0f,   // origin at (0,0)
        .origin_y = 0.0f    // origin at (0,0)
    };
    std::unique_ptr<int8_t[]> occupancy_grid = std::make_unique<int8_t[]>(width * height);

    for (int i = 0; i < width * height; ++i) {
        if (data.get()[i] == 0) {
            occupancy_grid[i] = 100;
        } else {
            occupancy_grid[i] = 0;
        }
    }
    
    int px_per_cell = 10;
    int grid_width = width * px_per_cell;
    int grid_height = height * px_per_cell;
    std::cout << "Grid size: " << grid_width << "x" << grid_height << "\n";

    RenderModule::Init(1400, 1200, 0.0);
    RenderModule::EnableRootWindowDocking();


    /* Create a FB and draw to it */
    int fb_width = width*px_per_cell;
    int fb_height = height*px_per_cell;
    
    // NVGLUframebuffer* fb = nvgluCreateFramebuffer(vg, width, height, NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY | NVG_IMAGE_FLIPY);
    nvg::SetContext(RenderModule::GetNanoVGContext());
    NVGLUframebuffer* fb = nvg::GLUtilsCreateFramebuffer(fb_width, fb_height, 
        // NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY | 
        // NVG_IMAGE_FLIPY
        // NVG_IMAGE_NEAREST
        0
    );
    if (!fb) {
        std::cerr << "Failed to create framebuffer." << std::endl;
        return -1;
    }
    // nvgImageSize(vg, fb->image, &width, &height);
    NVGpaint img_paint;
    auto render_grid_map = [&]() {
        nvg::GLUtilsBindFramebuffer(fb);
        glad::glViewport(0, 0, fb_width, fb_height);
        glad::glClearColor(0, 0, 0, 0);
        glad::glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        /* Draw Grid Map */
        nvg::BeginFrame(fb_width, fb_height, 1.0f);

            nvg::BeginPath();
            nvg::Rect(0, 0, static_cast<float>(grid_width), static_cast<float>(grid_height));
            nvg::FillColor(nvg::RGBAf(1.0f, 1.0f, 1.0f, 1.0f));
            nvg::Fill();

            for (int i = 0; i < width * height; ++i) {
                int x = (i % width) * px_per_cell;
                int y = (i / width) * px_per_cell;
                y = grid_height - y - px_per_cell; // Invert y-axis for correct rendering
                int8_t value = occupancy_grid.get()[i];
                if (value == 0) {
                    continue;
                }
                nvg::BeginPath();
                nvg::Rect(static_cast<float>(x), static_cast<float>(y), static_cast<float>(px_per_cell), static_cast<float>(px_per_cell));
                nvg::FillColor(nvg::RGBAf(0.6f, 0.6f, 0.6f, 1.0f));
                nvg::Fill();
            }

            nvg::BeginPath();
            // draw grid lines
            for (int i = 0; i <= grid_width; i+=px_per_cell) {
                nvg::MoveTo(i, 0);
                nvg::LineTo(i, grid_height);
            }
            for (int i = 0; i <= grid_height; i+=px_per_cell) {
                nvg::MoveTo(0, i);
                nvg::LineTo(grid_width, i);
            }
            nvg::StrokeColor(nvg::RGBAf(0.2f, 0.2f, 0.2f, 0.6f));
            nvg::StrokeWidth(0.8f);
            nvg::Stroke();

        nvg::EndFrame();
        nvg::GLUtilsBindFramebuffer(nullptr); 
            
        // NVGpaint img_paint = nvg::ImagePattern(0, 0, fb_width, fb_height, 0, fb->image, 1.0f);
        img_paint = nvg::ImagePattern(0, 0, fb_width, fb_height, 0, fb->image, 1.0f);
        if (img_paint.image == 0) {
            std::cerr << "Failed to create image pattern." << std::endl;
            return -1;
        }
        return 0;
    };
    render_grid_map();

    // nvg::GLUtilsBindFramebuffer(fb);
    // glad::glViewport(0, 0, fb_width, fb_height);
    // glad::glClearColor(0, 0, 0, 0);
    // glad::glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // /* Draw Grid Map */
    // nvg::BeginFrame(fb_width, fb_height, 1.0f);

    //     nvg::BeginPath();
    //     nvg::Rect(0, 0, static_cast<float>(grid_width), static_cast<float>(grid_height));
    //     nvg::FillColor(nvg::RGBAf(1.0f, 1.0f, 1.0f, 1.0f));
    //     nvg::Fill();

    //     for (int i = 0; i < width * height; ++i) {
    //         int x = (i % width) * px_per_cell;
    //         int y = (i / width) * px_per_cell;
    //         y = grid_height - y - px_per_cell; // Invert y-axis for correct rendering
    //         int8_t value = occupancy_grid.get()[i];
    //         if (value == 0) {
    //             continue;
    //         }
    //         nvg::BeginPath();
    //         nvg::Rect(static_cast<float>(x), static_cast<float>(y), static_cast<float>(px_per_cell), static_cast<float>(px_per_cell));
    //         nvg::FillColor(nvg::RGBAf(0.6f, 0.6f, 0.6f, 1.0f));
    //         nvg::Fill();
    //     }

    //     nvg::BeginPath();
    //     // draw grid lines
    //     for (int i = 0; i <= grid_width; i+=px_per_cell) {
    //         nvg::MoveTo(i, 0);
    //         nvg::LineTo(i, grid_height);
    //     }
    //     for (int i = 0; i <= grid_height; i+=px_per_cell) {
    //         nvg::MoveTo(0, i);
    //         nvg::LineTo(grid_width, i);
    //     }
    //     nvg::StrokeColor(nvg::RGBAf(0.2f, 0.2f, 0.2f, 0.6f));
    //     nvg::StrokeWidth(0.8f);
    //     nvg::Stroke();

    // nvg::EndFrame();
    // nvg::GLUtilsBindFramebuffer(nullptr); 
        
    // NVGpaint img_paint = nvg::ImagePattern(0, 0, fb_width, fb_height, 0, fb->image, 1.0f);
    // if (img_paint.image == 0) {
    //     std::cerr << "Failed to create image pattern." << std::endl;
    //     return -1;
    // }
    
    // RenderModule::RegisterNanoVGCallback("Red Circle", [&](NVGcontext* vg) {
    //     nvgBeginPath(vg);
    //     nvgRect(vg, 0, 0, 200, 200);
    //     nvgFillPaint(vg, img_paint);
    //     nvgFill(vg);
    //     nvgClosePath(vg);

    //     nvg::GLUtilsBindFramebuffer(fb);
    //     glad::glViewport(0, 0, fb_width, fb_height);
    //     // glad::glClearColor(0, 0, 0, 0);
    //     // glad::glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    //     // nvgSave(vg);
    //     // nvgluBindFramebuffer(fb);
    //     nvgBeginFrame(vg, fb_width, fb_height, 1.0f);
    //     nvgBeginPath(vg);
    //     static float cx = 10;
    //     static float cy = 10;
    //     static float dx = 0.5f;
    //     nvgCircle(vg, cx, cy, 5);
    //     cx += dx;
    //     cy += dx;
    //     nvgFillColor(vg, nvgRGBA(255, 0, 0, 155));
    //     nvgFill(vg);
    //     nvgClosePath(vg);
    //     nvgEndFrame(vg);
    //     nvgluBindFramebuffer(NULL);
    //     // nvgRestore(vg);
    // });

    // NVGLUframebuffer *fb = nvgluCreateFramebuffer(vg, grid_width, grid_height, NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY);
    // if (!fb) {
    //     std::cerr << "Failed to create framebuffer\n";
    //     return 1;
    // }
    // nvgluBindFramebuffer(fb);

    // nvgBeginPath(vg);
    // nvgCircle(vg, width/2, height/2, 50);
    // nvgFillColor(vg, nvgRGBA(255, 0, 0, 255));
    // nvgFill(vg);

    // nvgluBindFramebuffer(nullptr);


    std::random_device rd;
    std::mt19937 gen(rd());
    // std::uniform_real_distribution<float> dist(0.0f, std::static_cast<float>(fb_width));
    std::uniform_real_distribution<float> dist(0.0f, static_cast<float>(fb_width));

    float test = 0.5f;
    bool toggle = false;
    int count = 1;
    int square_count = 0;
    bool step = false;

    Pose start;
    Pose stop;
    PoseInteractionState pose_interaction_state = PoseInteractionState::kInactive;

    RenderModule::RegisterImGuiCallback([&]() {
        ImGui::Begin("Controls");
        // ImGui::Text("Grid Map Metadata:");
        ImGui::SliderFloat("Test", &test, 0.0f, 1.0f);
        ImGui::SliderInt("count", &count, 1, 1000);
        //create on off button
        if (ImGui::Button(toggle ? "STOP" : "RUN")) {
            toggle = !toggle;
        }
        if (ImGui::Button("Step")) {
            step = !step;
        }
        if (ImGui::Button("Reset")) {
            square_count = 0;
            render_grid_map();
        }
        if (ImGui::Button("Start Pose")) {
            pose_interaction_state = PoseInteractionState::kAwaitingStartClick;
            // start = Pose(); // Reset start pose
            RenderContext::Instance().disableViewportControls = true;
        }
        if (ImGui::Button("End Pose")) {
            // Start pose logic here
        }
        ImGui::End();

        // ImGui::Begin("Main Text Window", nullptr);
        // ImGui::Begin("Main Text Window");
        // ImGui::TextWrapped(
        //     "CJK text will only appear if the font was loaded with the appropriate CJK character ranges. "
        //     "Call io.Fonts->AddFontFromFileTTF() manually to load extra character ranges. "
        //     "Read docs/FONTS.md for details.");
        // ImGui::Text("Hiragana: \xe3\x81\x8b\xe3\x81\x8d\xe3\x81\x8f\xe3\x81\x91\xe3\x81\x93 (kakikukeko)");
        // ImGui::Text("Kanjis: \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e (nihongo)");
        // static char buf[32] = "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e";
        // ImGui::InputText("UTF-8 input", buf, IM_ARRAYSIZE(buf));
        // ImGui::End();
    });

    RenderModule::RegisterImGuiCallback([&]() {
        ImGui::Begin("Grid Map Viewer");
        ImGui::Text("Grid Map Metadata:");
        ImGui::Text("Width: %d, Height: %d, Resolution: %.2f m/pixel", map_metadata.width, map_metadata.height, map_metadata.resolution);
        ImGui::Text("Origin: (%.2f, %.2f)", map_metadata.origin_x, map_metadata.origin_y);
        ImGui::Text("Square count: %d", square_count);
        ImGui::Text(" ");
        ImGui::End();
    });

    RenderModule::RegisterNanoVGCallback("Grid Map Viewer", 
        /* Render */
        [&](NVGcontext* vg) {
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            bool pose_set = HandlePoseInteractionStateMachine(start, pose_interaction_state, canvasPos, canvasSize);
            if (pose_set) {
                pose_interaction_state = PoseInteractionState::kInactive;
                RenderContext::Instance().disableViewportControls = false;
                std::cout << "Start Pose Set: (" << start.x << ", " << start.y << ", " << start.dx << ", " << start.dy << ")\n";

            }





            // if (pose_interaction_state != PoseInteractionState::kInactive) {

            //     ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            //     ImVec2 canvasSize = ImGui::GetContentRegionAvail();

            //     ImVec2 cursorBackup = ImGui::GetCursorPos();

            //     ImGui::SetCursorScreenPos(canvasPos);
            //     ImGui::InvisibleButton("##MyBtn_1", canvasSize,
            //         ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

            //     ImGui::SetCursorPos(cursorBackup);

            //     bool hovering = ImGui::IsItemHovered();
            //     bool active = ImGui::IsItemActive();  
            //     ImVec2 delta = ImGui::GetIO().MouseDelta;

            //     std::cout << "Hovering: " << hovering << std::endl;
            //     std::cout << "Active: " << active << std::endl; 
            //     std::cout << "Mouse Delta: (" << delta.x << ", " << delta.y << ")" << std::endl;

            //     if (hovering && active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            //         start.dx += delta.x;
            //         start.dy -= delta.y; // Flip Y for NanoVG
            //     }
            //     std::cout << "Start Pose Delta: (" << start.dx << ", " << start.dy << ")" << std::endl;

            //     if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            //         start.x = ImGui::GetIO().MousePos.x - canvasPos.x;
            //         start.y = canvasSize.y - (ImGui::GetIO().MousePos.y - canvasPos.y);
            //         start.dx = 0.0f;
            //         start.dy = 0.0f;
            //     }
            //     start.theta = std::atan2(start.dy, start.dx);

            //     std::cout << "Start Pose: (" << start.x << ", " << start.y << ", " << start.theta << ")" << std::endl;

            // }
            RenderModule::ZoomView([&](NVGcontext* vg) {
                nvg::BeginPath();
                nvg::Rect(0, 0, fb_width, fb_height);
                nvg::FillPaint(img_paint);
                nvg::Fill();

                float scale = ZoomView::GetScale();
                ImVec2 offset = ZoomView::GetOffset();
                std::cout << "Zoom Scale: " << scale << ", Offset: (" << offset.x << ", " << offset.y << ")\n";

                Pose backup = start;
                // if (pose_interaction_state == PoseInteractionState::kAwaitingDragDirection ||
                    // pose_interaction_state == PoseInteractionState::kAwaitingStartClick) {
                    
                    start.x = (start.x - offset.x) * scale;
                    start.y = (start.y - offset.y) * scale;
                    start.dx *= scale;
                    start.dy *= scale;
                // }


            if (pose_interaction_state == PoseInteractionState::kAwaitingDragDirection ||
                pose_interaction_state == PoseInteractionState::kComplete || 
                start.active) {
                nvg::BeginPath();
                nvg::MoveTo(start.x, start.y);
                nvg::LineTo(start.x + start.dx, start.y + start.dy);
                nvg::StrokeColor(nvg::RGBAf(0.3f, 0.3f, 0.3f, 1.0f));
                nvg::StrokeWidth(2.0f);
                nvg::Stroke();
                // nvg::BeginPath();
                // nvg::Circle(start.x, start.y, 5.0f);
                // nvg::FillColor(nvg::RGBAf(0.3f, 0.3f, 0.3f, 1.0f));
                // nvg::Fill();
                nvg::BeginPath();
                // nvg::Circle(start.x + start.dx, start.y + start.dy, 5.0f);
                nvg::Circle(start.x, start.y, 5.0f);
                // nvg::ClosePath();
                nvg::FillColor(nvg::RGBAf(0.0f, 1.0f, 0.0f, 1.0f));
                nvg::Fill();
            }
                start = backup;
            });

            // RenderModule::ZoomView([&](NVGcontext* vg) {
            // });

        },
        /* Offscreen Render */
        [&](NVGcontext* vg) {
            RenderModule::IsolatedFrameBuffer([&](NVGcontext* vg) {
                if (toggle || step) {
                    nvg::GLUtilsBindFramebuffer(fb);
                    glad::glViewport(0, 0, fb_width, fb_height);

                    nvg::BeginFrame(fb_width, fb_height, 1.0f);
                    nvg::BeginPath();
                    for (int i = 0; i < count; ++i) {
                        // nvg::Rect(std::floor(dist(gen)), std::floor(dist(gen)), px_per_cell*test, px_per_cell*test);
                        int x = static_cast<int>(dist(gen));
                        int y = static_cast<int>(dist(gen));
                        x /= 10;
                        y /= 10;
                        nvg::Rect(x*10, y*10, px_per_cell*test, px_per_cell*test);
                        square_count++;
                    }
                    nvg::FillColor(nvg::RGBAf(1.0f, 0.0f, 0.0f, 1.0f));
                    nvg::Fill();
                    nvg::EndFrame();
                    nvg::GLUtilsBindFramebuffer(nullptr);
                    step = false;
                }
            });
        }
    );

    RenderModule::Run();
    RenderModule::Shutdown(); 
    // nvgluDeleteFramebuffer(fb);


    return 0;
}