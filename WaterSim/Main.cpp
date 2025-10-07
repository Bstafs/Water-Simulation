#include "Application.h"
#include "Timestep.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize the application
    Application* theApp = new Application();
    Timestep deltaTime(60.0f);

    if (FAILED(theApp->Initialise(hInstance, nCmdShow)))
    {
        delete theApp;
        return -1;
    }

    MSG msg = { 0 };

    // Main message and game loop
    while (true)
    {
        // Process all pending messages
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                break; // Exit the loop if the quit message is received
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // If we received a quit message, break the main loop
        if (msg.message == WM_QUIT)
        {
            break;
        }

        // Calculate timestep
        deltaTime.CalculateTimestep();

        // Fixed timestep updates
        while (deltaTime.IsReadyForPhysicsUpdate())
        {
            theApp->UpdatePhysics(deltaTime.GetFixedTimeStep()); // Physics
            deltaTime.ConsumePhysicsUpdate();
        }

        // Handle input, game logic, and rendering
        theApp->HandleKeyboard(deltaTime.GetFixedTimeStep());    // Input handling   

        theApp->Update();           // Camera
        theApp->Draw();             // Rendering
       // theApp->DrawMarchingCubes();             // Rendering
    }

    // Cleanup
    delete theApp;
    return (int)msg.wParam;
}