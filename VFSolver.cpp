#include <iostream>
#include <string>
#include <format>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <stdio.h>
#include <SDL.h>
#ifdef _WIN32
#include <windows.h>        // SetProcessDPIAware()
#endif

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif


bool showDemoWindow = true;


#include "VF\Solver.h"

int columns = 5;
int rows = 5;
VF::Solver* p_Solver = nullptr;
unsigned int* uPoint = nullptr;
unsigned int* vPoint = nullptr;
unsigned int* uVolt = nullptr;
unsigned int* vVolt = nullptr;
bool isValidBoard = false;


static void OnImGuiRender(ImVec4& clear_color, ImGuiIO& io)
{
	{
		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
		ImGui::SetNextWindowSize(io.DisplaySize);

		if (ImGui::Begin("main", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoBringToFrontOnFocus
		)
		)
		{
			if (p_Solver == nullptr)
			{
				//	Solver has not been created
				ImGui::InputInt("Rows", &rows, 1, 0);
				if (rows < 0)
					rows = 0;

				ImGui::InputInt("Columns", &columns, 1, 0);
				if (columns < 0)
					columns = 0;

				if (ImGui::Button("Create field"))
				{
					p_Solver = new VF::Solver(columns, rows);
					uPoint = new unsigned int[columns] {};
					vPoint = new unsigned int[rows] {};
					uVolt = new unsigned int[columns] {};
					vVolt = new unsigned int[rows] {};
				}
			}
			else
			{
				//	Solver has been created
				if (ImGui::Button("Delete field"))
				{
					delete p_Solver;
					delete[] uPoint;
					delete[] vPoint;
					delete[] uVolt;
					delete[] vVolt;
					p_Solver = nullptr;
					uPoint = nullptr;
					vPoint = nullptr;
					uVolt = nullptr;
					vVolt = nullptr;
				}

				if (p_Solver)
				{
					if (ImGui::BeginTable("##tableSolver", 2, ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_BordersOuter))
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);

						//	Board
						if (ImGui::BeginTable("##tableBoard", columns, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInner, ImVec2(0.0f, 0.0f), 250.0f))
						{
							for (unsigned int v = 0; v < rows; v++)
							{
								ImGui::TableNextRow();
								for (unsigned int u = 0; u < columns; u++)
								{
									ImGui::TableSetColumnIndex(u);

									int memo = p_Solver->GetMemo(u, v);

									auto VFButton = [memo, u, v](VF::Solver::MEMO_TYPE _type) mutable
									{
										bool isPressed = false;

										constexpr ImVec2 buttonSize = { 50, 50 };
										std::string buttonName = "";
										switch (_type)
										{
										case VF::Solver::MEMO_1:
											buttonName = std::format("1##{},{}", u, v); break;
										case VF::Solver::MEMO_2:
											buttonName = std::format("2##{},{}", u, v); break;
										case VF::Solver::MEMO_3:
											buttonName = std::format("3##{},{}", u, v); break;
										case VF::Solver::MEMO_VOLT:
											buttonName = std::format("V##{},{}", u, v); break;
										}

										if (memo & _type)
										{
											ImGui::PushStyleColor(ImGuiCol_Button, p_Solver->IsMemoUserConfirmed(u, v) ? ImVec4(0.0f, 0.75f, 0.0f, 1.0f) : ImVec4(0.0f, 0.25f, 0.5f, 1.0f));
											isPressed = ImGui::Button(buttonName.c_str(), buttonSize);
											ImGui::PopStyleColor();
										}
										else
										{
											ImGui::InvisibleButton(buttonName.c_str(), buttonSize);
										}

										if (isValidBoard && isPressed)
										{
											if (p_Solver->IsMemoUserConfirmed(u, v))
												memo = p_Solver->UnsetMemo(u, v);
											else
												memo = p_Solver->SetMemo(u, v, _type);
										}

										return isPressed;
									};

									VFButton(VF::Solver::MEMO_1);

									ImGui::SameLine();
									VFButton(VF::Solver::MEMO_2);

									VFButton(VF::Solver::MEMO_3);

									ImGui::SameLine();
									VFButton(VF::Solver::MEMO_VOLT);
								}
							}

							ImGui::EndTable();
						}

						ImGui::TableSetColumnIndex(1);

						bool isHintsChanged = false;

						//	Hints (right)
						if (ImGui::BeginTable("##tableHintsRight", 1, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuter))
						{
							for (unsigned int v = 0; v < rows; v++)
							{
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);

								ImGui::PushItemWidth(50);

								if (ImGui::InputInt(std::format("##p{}", v).c_str(), (int*)(vPoint + v), 0, 0))
									isHintsChanged = true;
								if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
								{
									ImGui::SetTooltip("Points");
								}

								if (ImGui::InputInt(std::format("##v{}", v).c_str(), (int*)(vVolt + v), 0, 0))
									isHintsChanged = true;
								if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
								{
									ImGui::SetTooltip("Volts");
								}

								ImGui::PopItemWidth();
							}

							ImGui::EndTable();
						}

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);

						//	Hints (bottom)
						if (ImGui::BeginTable("##tableHintsBottom", columns, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter))
						{
							ImGui::TableNextRow();

							for (unsigned int u = 0; u < columns; u++)
							{
								ImGui::TableSetColumnIndex(u);

								ImGui::PushItemWidth(50);

								if (ImGui::InputInt(std::format("##p{}", u).c_str(), (int*)(uPoint + u), 0, 0))
									isHintsChanged = true;
								if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
								{
									ImGui::SetTooltip("Points");
								}

								if (ImGui::InputInt(std::format("##v{}", u).c_str(), (int*)(uVolt + u), 0, 0))
									isHintsChanged = true;
								if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
								{
									ImGui::SetTooltip("Volts");
								}

								ImGui::PopItemWidth();
							}

							ImGui::EndTable();
						}

						if (isHintsChanged)
						{
							isValidBoard = p_Solver->SetHints(uPoint, vPoint, uVolt, vVolt);
						}

						ImGui::EndTable();
					}
				}
			}

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
		}
		ImGui::End();
	}

	if (showDemoWindow)
		ImGui::ShowDemoWindow(&showDemoWindow);
}

// Main code
int main(int, char**)
{
	// Setup SDL
#ifdef _WIN32
	::SetProcessDPIAware();
#endif
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
	{
		printf("Error: %s\n", SDL_GetError());
		return -1;
	}

	// From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

	// Create window with SDL_Renderer graphics context
	float main_scale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int)(1280 * main_scale), (int)(720 * main_scale), window_flags);
	if (window == nullptr)
	{
		printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
		return -1;
	}
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
	if (renderer == nullptr)
	{
		SDL_Log("Error creating SDL_Renderer!");
		return -1;
	}
	//SDL_RendererInfo info;
	//SDL_GetRendererInfo(renderer, &info);
	//SDL_Log("Current SDL_Renderer: %s", info.name);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup scaling
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer2_Init(renderer);

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
	// - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	//style.FontSizeBase = 20.0f;
	//io.Fonts->AddFontDefault();
	//io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
	//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
	//IM_ASSERT(font != nullptr);

	// Our state
	bool show_demo_window = true;
	bool show_another_window = false;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);


	/*Solve(5, 5, uPoints, vPoints, uVolts, vVolts, memos);

	memos[0 * 5 + 4] = MEMO_1 | MEMO_CONF;
	memos[1 * 5 + 4] = MEMO_2 | MEMO_CONF;
	memos[2 * 5 + 4] = MEMO_1 | MEMO_CONF;
	memos[3 * 5 + 4] = MEMO_1 | MEMO_CONF;
	memos[4 * 5 + 4] = MEMO_2 | MEMO_CONF;

	Solve(5, 5, uPoints, vPoints, uVolts, vVolts, memos);

	delete[] memos;*/



	// Main loop
	bool done = false;
	while (!done)
	{
		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				done = true;
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
				done = true;
		}
		if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
		{
			SDL_Delay(10);
			continue;
		}

		// Start the Dear ImGui frame
		ImGui_ImplSDLRenderer2_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		OnImGuiRender(clear_color, io);

		// Rendering
		ImGui::Render();
		SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
		SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
		SDL_RenderPresent(renderer);
	}

	// Cleanup
	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}