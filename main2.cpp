#include "stdafx.h"
#include <iostream>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_dx12.h"

// Disable OpenGL checks in GLFW
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "d3dx12.h" 
#include "RenderWidgets/RenderingOrderExp.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

using namespace Microsoft::WRL;

// Globals
const int NUM_FRAMES = 2;
ComPtr<ID3D12Device> g_device;
ComPtr<ID3D12CommandQueue> g_queue;
ComPtr<IDXGISwapChain3> g_swapChain;
ComPtr<ID3D12DescriptorHeap> g_rtvHeap;
ComPtr<ID3D12DescriptorHeap> g_dsvHeap;
ComPtr<ID3D12CommandAllocator> g_alloc[NUM_FRAMES];
ComPtr<ID3D12GraphicsCommandList> g_cmdList;
ComPtr<ID3D12Resource> g_rtvs[NUM_FRAMES];
ComPtr<ID3D12Resource> g_dsv;
ComPtr<ID3D12Fence> g_fence;
UINT64 g_fenceValue = 0;
HANDLE g_fenceEvent;
UINT g_frameIndex = 0;
UINT g_rtvSize = 0;
static bool g_pendingResize = false;
static UINT g_pendingWidth = 0;
static UINT g_pendingHeight = 0;
static GLFWmousebuttonfun g_prevMouseButtonCallback = nullptr;
static UINT64 g_frameFenceValues[NUM_FRAMES] = {};
ComPtr<ID3D12DescriptorHeap> g_imguiSrvHeap;
static bool g_imguiSrvInUse = false;
static UINT g_imguiSrvIncrement = 0;
INANOA::RenderingOrderExp* renderer = nullptr;

// ============================
// Camera input helpers 
// ============================
static bool g_rmbDown = false;
static double g_lastMouseX = 0.0;
static double g_lastMouseY = 0.0;

enum class ActiveView { None, God, Player };
static ActiveView g_activeView = ActiveView::God;

// Extract camera basis from its view matrix (LH, Z-forward)
static glm::vec3 CameraForwardXZ(INANOA::Camera* cam)
{
    glm::mat4 vmt = glm::transpose(cam->viewMatrix());
    glm::vec3 f(vmt[2].x, 0.0f, vmt[2].z);
    if (glm::length(f) < 0.0001f) return glm::vec3(0, 0, 1);
    return glm::normalize(f);
}

static glm::vec3 CameraRightXZ(INANOA::Camera* cam)
{
    glm::mat4 vmt = glm::transpose(cam->viewMatrix());
    glm::vec3 r(vmt[0].x, vmt[0].y, vmt[0].z);
    if (glm::length(r) < 0.0001f) return glm::vec3(1, 0, 0);
    return glm::normalize(r);
}

static void RotateGodCameraMouseLook(INANOA::Camera* cam, double dx, double dy, float sensitivityRadPerPixel)
{
    // yaw around camera up (you already have helper)
    cam->rotateLookCenterAccordingToViewOrg(static_cast<float>(dx) * sensitivityRadPerPixel);

    // pitch around camera right axis using same pattern as Camera::rotateLookCenterAccordingToViewOrg
    glm::mat4 vmt = glm::transpose(cam->viewMatrix());
    glm::vec3 right(-vmt[0].x, vmt[0].y, vmt[0].z);
    if (glm::length(right) < 0.0001f) return;
    right = glm::normalize(right);

    glm::quat q = glm::angleAxis(static_cast<float>(dy) * sensitivityRadPerPixel, right);
    glm::mat4 rotMat = glm::toMat4(q);

    const glm::vec3 eye = cam->viewOrig();
    const glm::vec3 center = cam->lookCenter(); // requires getter; see note below

    glm::vec3 p = center - eye;
    glm::vec4 rp = rotMat * glm::vec4(p, 1.0f);

    cam->setLookCenter(glm::vec3(rp) + eye);
}

// Helper to wait for GPU
void WaitForGpu() {
    g_queue->Signal(g_fence.Get(), ++g_fenceValue);
    if (g_fence->GetCompletedValue() < g_fenceValue) {
        g_fence->SetEventOnCompletion(g_fenceValue, g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

static void WaitForFrame(UINT frameIndex)
{
    const UINT64 fenceToWaitFor = g_frameFenceValues[frameIndex];
    if (fenceToWaitFor != 0 && g_fence->GetCompletedValue() < fenceToWaitFor)
    {
        g_fence->SetEventOnCompletion(fenceToWaitFor, g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

static void ResizeSwapChain(UINT newWidth, UINT newHeight)
{
    // Ensure GPU is not using the resources we are about to destroy.
    // This is the one place we deliberately do a full GPU wait.
    WaitForGpu();

    // Release back buffers before resizing
    for (int i = 0; i < NUM_FRAMES; ++i)
        g_rtvs[i].Reset();

    // Resize swap chain buffers
    DXGI_SWAP_CHAIN_DESC desc = {};
    g_swapChain->GetDesc(&desc);

    HRESULT hr = g_swapChain->ResizeBuffers(
        NUM_FRAMES,
        newWidth,
        newHeight,
        desc.BufferDesc.Format,
        desc.Flags);

    if (FAILED(hr))
        return;

    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    // Recreate RTVs
    g_rtvSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (int i = 0; i < NUM_FRAMES; i++)
    {
        g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_rtvs[i]));
        g_device->CreateRenderTargetView(g_rtvs[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, g_rtvSize);
    }

    // Recreate DSV resource/view
    g_dsv.Reset();

    D3D12_RESOURCE_DESC dsResDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_D32_FLOAT,
            newWidth,
            newHeight,
            1, 1, 1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    D3D12_CLEAR_VALUE clearVal = { DXGI_FORMAT_D32_FLOAT, { 1.0f, 0 } };

    g_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &dsResDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearVal,
        IID_PPV_ARGS(&g_dsv));

    g_device->CreateDepthStencilView(
        g_dsv.Get(),
        nullptr,
        g_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    // Notify your app renderer so it updates split viewports/cameras.
    if (renderer)
        renderer->resize(static_cast<int>(newWidth), static_cast<int>(newHeight));

    // Reset frame fence bookkeeping (safe after full wait)
    for (int i = 0; i < NUM_FRAMES; ++i)
        g_frameFenceValues[i] = 0;
}

static void on_framebuffer_size(GLFWwindow* window, int width, int height)
{
    // Minimized window reports 0,0 — ignore until restored
    if (width <= 0 || height <= 0)
        return;

    g_pendingResize = true;
    g_pendingWidth = static_cast<UINT>(width);
    g_pendingHeight = static_cast<UINT>(height);
}

// Minimal D3D12 Init
bool InitD3D(HWND hwnd, int tempW, int tempH) {
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device));

    D3D12_COMMAND_QUEUE_DESC qDesc = {};
    qDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    g_device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&g_queue));

    // ImGui SRV heap (shader-visible) - create heap ONLY (no ImGui calls here)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        g_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_imguiSrvHeap));
    }

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.BufferCount = NUM_FRAMES;
    scDesc.Width = tempW; scDesc.Height = tempH;
    scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.SampleDesc.Count = 1;

    ComPtr<IDXGIFactory4> factory;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    ComPtr<IDXGISwapChain1> swap1;
    factory->CreateSwapChainForHwnd(g_queue.Get(), hwnd, &scDesc, nullptr, nullptr, &swap1);
    swap1.As(&g_swapChain);

    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    // RTV Heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_FRAMES, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
    g_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&g_rtvHeap));
    g_rtvSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // DSV Heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
    g_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&g_dsvHeap));

    // RTVs
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (int i = 0; i < NUM_FRAMES; i++) {
        g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_rtvs[i]));
        g_device->CreateRenderTargetView(g_rtvs[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, g_rtvSize);
    }

    // DSV
    D3D12_RESOURCE_DESC dsResDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, tempW, tempH, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    D3D12_CLEAR_VALUE clearVal = { DXGI_FORMAT_D32_FLOAT, { 1.0f, 0 } };
    g_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &dsResDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal, IID_PPV_ARGS(&g_dsv));
    g_device->CreateDepthStencilView(g_dsv.Get(), nullptr, g_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    // Allocators
    for (int i = 0; i < NUM_FRAMES; i++) g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_alloc[i]));

    g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_alloc[0].Get(), nullptr, IID_PPV_ARGS(&g_cmdList));
    g_cmdList->Close();

    g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence));
    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    return true;
}

static void on_mouse_button(GLFWwindow* w, int button, int action, int mods)
{
    // Forward to ImGui (and any other previously installed callback)
    if (g_prevMouseButtonCallback)
        g_prevMouseButtonCallback(w, button, action, mods);

    // If ImGui wants to capture the mouse, don't control the camera.
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    // Your camera selection logic
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        g_rmbDown = (action == GLFW_PRESS);

        double x, y;
        glfwGetCursorPos(w, &x, &y);
        g_lastMouseX = x;
        g_lastMouseY = y;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        double x, y;
        glfwGetCursorPos(w, &x, &y);

        int ww, hh;
        glfwGetFramebufferSize(w, &ww, &hh);
        const int halfW = ww / 2;

        g_activeView = (x < halfW) ? ActiveView::God : ActiveView::Player;
    }
}

void camera_input_update(GLFWwindow* window, float dt) {
    // (moved into main loop for access to cameras)
    // Access cameras (add getters on RenderingOrderExp if needed)
    INANOA::Camera* godCam = renderer->godCamera();
    INANOA::Camera* playerCam = renderer->playerCamera();

    if (ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantCaptureMouse)
        return;

    // Mouse look only when god view is active and RMB held
    if (g_activeView == ActiveView::God && g_rmbDown)
    {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        double dx = mx - g_lastMouseX;
        double dy = my - g_lastMouseY;
        g_lastMouseX = mx;
        g_lastMouseY = my;

        const float sens = 0.0035f; // rad/pixel
        RotateGodCameraMouseLook(godCam, dx, dy, sens);
    }

    // Keyboard movement: WASD works on active view camera; QE only for god flycam
    const float speed = 25.0f * dt;

    INANOA::Camera* target = (g_activeView == ActiveView::God) ? godCam :
        (g_activeView == ActiveView::Player) ? playerCam : nullptr;

    if (target)
    {
        glm::vec3 f = CameraForwardXZ(target);
        glm::vec3 r = CameraRightXZ(target);

        glm::vec3 delta(0.0f);

        // Forward/back always moves (both views)
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) delta += f * speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) delta -= f * speed;

        if (g_activeView == ActiveView::God)
        {
            // God view: A/D strafe
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) delta += r * speed;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) delta -= r * speed;

            // Vertical fly (you used Z/X)
            if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) delta.y += speed;
            if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) delta.y -= speed;
        }
        else if (g_activeView == ActiveView::Player)
        {
            // Player view: A/D turn (yaw), not strafe
            const float turnSpeed = 1.8f; // radians/sec (tune)
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) target->rotateLookCenterAccordingToViewOrg(-turnSpeed * dt);
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) target->rotateLookCenterAccordingToViewOrg(turnSpeed * dt);
        }

        if (glm::length(delta) > 0.0f)
            target->translateLookCenterAndViewOrg(delta);
    }
}

void on_gui() {
    
    // FPS / MS window
    {
        ImGuiIO& io = ImGui::GetIO();
        const float fps = io.Framerate;
        const float ms = (fps > 0.0f) ? (1000.0f / fps) : 0.0f;

        ImGui::Begin("Information");
        ImGui::Text("fps: %.2f", fps);
        ImGui::Text("ms:  %.2f", ms);
        ImGui::End();
    }
    
}

int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "D3D12", nullptr, nullptr);
    HWND hwnd = glfwGetWin32Window(window);

    InitD3D(hwnd, 1280, 720);
    glfwSetFramebufferSizeCallback(window, on_framebuffer_size);
    // Setup Dear ImGui context (must come BEFORE backend init)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // Platform backend (GLFW)
    ImGui_ImplGlfw_InitForOther(window, true);

    // Renderer backend (DX12) - init ONCE with allocator callbacks
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = g_device.Get();
    init_info.CommandQueue = g_queue.Get();
    init_info.NumFramesInFlight = NUM_FRAMES;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    init_info.SrvDescriptorHeap = g_imguiSrvHeap.Get();

    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu)
        {
            IM_ASSERT(!g_imguiSrvInUse);
            g_imguiSrvInUse = true;
            *out_cpu = g_imguiSrvHeap->GetCPUDescriptorHandleForHeapStart();
            *out_gpu = g_imguiSrvHeap->GetGPUDescriptorHandleForHeapStart();
        };

    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE)
        {
            IM_ASSERT(g_imguiSrvInUse);
            g_imguiSrvInUse = false;
        };

    ImGui_ImplDX12_Init(&init_info);

    // Initialize Renderer
    renderer = new INANOA::RenderingOrderExp();
    if (!renderer->init(g_device.Get(), 1280, 720)) {
        std::cout << "Renderer Init Failed (Shader Error likely)" << std::endl;
        return 1;
    }

    g_prevMouseButtonCallback = glfwSetMouseButtonCallback(window, on_mouse_button);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (g_pendingResize)
        {
            g_pendingResize = false;
            ResizeSwapChain(g_pendingWidth, g_pendingHeight);
        }

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        on_gui();

        static double lastTime = glfwGetTime();
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        // clamp dt for stability during debugging/breakpoints
        if (dt > 0.05f) dt = 0.05f;

        //glfwSetWindowUserPointer(window, window);
		camera_input_update(window, dt);
        
        // Now update scene (this will update frustum from player camera)
        renderer->update();
        WaitForFrame(g_frameIndex);
        // Render Frame
        g_alloc[g_frameIndex]->Reset();
        g_cmdList->Reset(g_alloc[g_frameIndex].Get(), nullptr);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(g_rtvHeap->GetCPUDescriptorHandleForHeapStart(), g_frameIndex, g_rtvSize);
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsv(g_dsvHeap->GetCPUDescriptorHandleForHeapStart());

        g_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_rtvs[g_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        float clearCol[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        g_cmdList->ClearRenderTargetView(rtv, clearCol, 0, nullptr);
        g_cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        g_cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

        D3D12_VIEWPORT vp = { 0, 0, 1280, 720, 0, 1 };
        D3D12_RECT sc = { 0, 0, 1280, 720 };
        g_cmdList->RSSetViewports(1, &vp);
        g_cmdList->RSSetScissorRects(1, &sc);

        // Call User Renderer
        renderer->render(g_cmdList.Get(), rtv, dsv);
        ImGui::Render();

        // ImGui uses its own SRV heap, must be bound before rendering
        ID3D12DescriptorHeap* heaps[] = { g_imguiSrvHeap.Get() };
        g_cmdList->SetDescriptorHeaps(1, heaps);

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_cmdList.Get());

        g_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_rtvs[g_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        g_cmdList->Close();

        ID3D12CommandList* lists[] = { g_cmdList.Get() };
        g_queue->ExecuteCommandLists(1, lists);
        g_swapChain->Present(0, 0);

        g_queue->Signal(g_fence.Get(), ++g_fenceValue);
        g_frameFenceValues[g_frameIndex] = g_fenceValue;
      
        g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();
    }

    return 0;
}