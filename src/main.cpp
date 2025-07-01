#include <iostream>
#include <memory>
#include <random>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

#include "render_module/render_module.hpp" 
#include "render_module/glad_wrapper.hpp"


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
    bool transformed = false;
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
                    pose.active = false;
                    pose.transformed = false;
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
                    pose.theta = atan2(pose.dy, pose.dx);
                }

                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    state = PoseInteractionState::kComplete;
                    interactionComplete = true;
                    pose.active = true;  // Mark pose as active
                }
                break;

            case PoseInteractionState::kComplete:
                pose.active = true;  
                // Optional: reset or wait for external signal
                break;
        }

        if (state == PoseInteractionState::kAwaitingDragDirection) {

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

    RenderModule::Init(1500, 1200, 0.0);
    RenderModule::EnableRootWindowDocking();
    RenderModule::EnableDebugConsole();
    RenderModule::Console().SetCoutRedirect(true);


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
    auto render_grid_map = [&](bool offscreen = true) {
        if (offscreen) {
            nvg::GLUtilsBindFramebuffer(fb);
            glad::glViewport(0, 0, fb_width, fb_height);
            glad::glClearColor(0, 0, 0, 0);
            glad::glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

            /* Draw Grid Map */
            nvg::BeginFrame(fb_width, fb_height, 1.0f);
        }

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
        
        if (offscreen) {
            nvg::EndFrame();
            nvg::GLUtilsBindFramebuffer(nullptr); 
                
            // NVGpaint img_paint = nvg::ImagePattern(0, 0, fb_width, fb_height, 0, fb->image, 1.0f);
            img_paint = nvg::ImagePattern(0, 0, fb_width, fb_height, 0, fb->image, 1.0f);
            if (img_paint.image == 0) {
                std::cerr << "Failed to create image pattern." << std::endl;
                return -1;
            }
        }
        return 0;
    };
    render_grid_map();


    std::random_device rd;
    std::mt19937 gen(rd());
    // std::uniform_real_distribution<float> dist(0.0f, std::static_cast<float>(fb_width));
    std::uniform_real_distribution<float> dist(0.0f, static_cast<float>(fb_width));
    // std::uniform_real_distribution<float> dist(0.0f, static_cast<float>(10));

    float test = 0.5f;
    bool toggle = false;
    int count = 1;
    int square_count = 0;
    bool step = false;

    Pose start;
    Pose end;
    PoseInteractionState pose_start_interaction_state = PoseInteractionState::kInactive;
    PoseInteractionState pose_end_interaction_state = PoseInteractionState::kInactive;

    auto paint_pose = [&](NVGcontext* vg, const Pose& pose, bool is_end = false) {
        nvg::BeginPath();
        nvg::MoveTo(pose.x, pose.y);
        nvg::LineTo(pose.x + pose.dx, pose.y + pose.dy);
        nvg::StrokeColor(nvg::RGBAf(0.3f, 0.3f, 0.3f, 1.0f));
        nvg::StrokeWidth(2.0f);
        nvg::Stroke();
        nvg::BeginPath();
        nvg::Circle(pose.x, pose.y, 4.0f);
        if (!is_end) {
            nvg::FillColor(nvg::RGBAf(0.0f, 1.0f, 0.0f, 1.0f));
        } else {
            nvg::FillColor(nvg::RGBAf(1.0f, 0.0f, 0.0f, 1.0f));
        }
        nvg::Fill();
    };

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
            pose_start_interaction_state = PoseInteractionState::kAwaitingStartClick;
            RenderContext::Instance().disableViewportControls = true;
        }
        if (ImGui::Button("End Pose")) {
            pose_end_interaction_state = PoseInteractionState::kAwaitingStartClick;
            RenderContext::Instance().disableViewportControls = true;
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
        // ImGui::Text("Grid Map Metadata:");
        ImGui::Text("Width: %d, Height: %d, Resolution: %.2f m/pixel", map_metadata.width, map_metadata.height, map_metadata.resolution);
        // ImGui::Text("Origin: (%.2f, %.2f)", map_metadata.origin_x, map_metadata.origin_y);
        ImGui::Text("Square count: %d", square_count);
        // ImGui::Text("FPS: %.2f", RenderModule::GetFPS());
        // ImGui::Text(" ");
        // ImGui::NewLine();
        ImGui::Separator();
        ImGui::End();
    });

    RenderModule::RegisterNanoVGCallback("Grid Map Viewer", 
        /* Render */
        [&](NVGcontext* vg) {

            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            {
                bool pose_set = HandlePoseInteractionStateMachine(start, pose_start_interaction_state, canvasPos, canvasSize);
                if (pose_set) {
                    pose_start_interaction_state = PoseInteractionState::kInactive;
                    RenderContext::Instance().disableViewportControls = false;
                    std::cout << "Start Pose: (" << start.x << ", " << start.y << ", " << start.theta << ")\n";
                }
            }
            {
                bool pose_set = HandlePoseInteractionStateMachine(end, pose_end_interaction_state, canvasPos, canvasSize);
                if (pose_set) {
                    pose_end_interaction_state = PoseInteractionState::kInactive;
                    RenderContext::Instance().disableViewportControls = false;
                    std::cout << "End Pose: (" << end.x << ", " << end.y << ", " << end.theta << ")\n";
                }
            }

            RenderModule::ZoomView([&](NVGcontext* vg) {
                /* Render Pre-Computed Background */
                // nvg::BeginPath();
                // nvg::Rect(0, 0, fb_width, fb_height);
                // nvg::FillPaint(img_paint);
                // nvg::Fill();
                render_grid_map(false);

                /* Transform once from canvas to view */
                if (start.active && !start.transformed) {
                    std::cout << "Transforming start pose to view coordinates.\n";
                    ZoomView::CanvasToView(start.x, start.y);
                    ZoomView::CanvasToView(start.dx);
                    ZoomView::CanvasToView(start.dy);
                    start.transformed = true;
                    std::cout << "Transformed Start Pose: (" << start.x << ", " << start.y << ", " << start.theta << ")\n";
                }
                if (end.active && !end.transformed) {
                    std::cout << "Transforming end pose to view coordinates.\n";
                    ZoomView::CanvasToView(end.x, end.y);
                    ZoomView::CanvasToView(end.dx);
                    ZoomView::CanvasToView(end.dy);
                    end.transformed = true;
                    std::cout << "Transformed End Pose: (" << end.x << ", " << end.y << ", " << end.theta << ")\n";
                }

                if (start.transformed) {
                    paint_pose(vg, start);
                }
                if (end.transformed) {
                    paint_pose(vg, end, true);
                }

                static std::vector<float> x_coords;
                static std::vector<float> y_coords;
                if (toggle || step) {
                    // nvg::GLUtilsBindFramebuffer(fb);
                    // glad::glViewport(0, 0, fb_width, fb_height);

                    // nvg::BeginFrame(fb_width, fb_height, 1.0f);

                    for (int i = 0; i < count; ++i) {
                        x_coords.emplace_back(dist(gen));
                        y_coords.emplace_back(dist(gen));
                        square_count++;
                    }
                    // nvg::EndFrame();
                    // nvg::GLUtilsBindFramebuffer(nullptr);
                    step = false;
                }
                for (int i = 0; i < x_coords.size(); ++i) {
                    nvg::BeginPath();
                    nvg::Rect(x_coords[i], y_coords[i], px_per_cell*test, px_per_cell*test);
                    nvg::FillColor(nvg::RGBAf(1.0f, 0.0f, 0.0f, 1.0f));
                    nvg::Fill();
                }

            });

            /* Paint orientation during interactions */
            if (pose_start_interaction_state == PoseInteractionState::kAwaitingDragDirection) {
                paint_pose(vg, start);
            }
            if (pose_end_interaction_state == PoseInteractionState::kAwaitingDragDirection) {
                paint_pose(vg, end, true);
            }



        },
        /* Offscreen Render */
        [&](NVGcontext* vg) {
            // RenderModule::IsolatedFrameBuffer([&](NVGcontext* vg) {
            //     if (toggle || step) {
            //         nvg::GLUtilsBindFramebuffer(fb);
            //         glad::glViewport(0, 0, fb_width, fb_height);

            //         nvg::BeginFrame(fb_width, fb_height, 1.0f);
            //         nvg::BeginPath();
            //         for (int i = 0; i < count; ++i) {
            //             // nvg::Rect(std::floor(dist(gen)), std::floor(dist(gen)), px_per_cell*test, px_per_cell*test);
            //             int x = static_cast<int>(dist(gen));
            //             int y = static_cast<int>(dist(gen));
            //             x /= 10;
            //             y /= 10;
            //             nvg::Rect(x*10, y*10, px_per_cell*test, px_per_cell*test);
            //             square_count++;
            //         }
            //         nvg::FillColor(nvg::RGBAf(1.0f, 0.0f, 0.0f, 1.0f));
            //         nvg::Fill();
            //         nvg::EndFrame();
            //         nvg::GLUtilsBindFramebuffer(nullptr);
            //         step = false;
            //     }
            // });
        }
    );

    RenderModule::Run();
    RenderModule::Shutdown(); 
    // nvgluDeleteFramebuffer(fb);


    return 0;
}