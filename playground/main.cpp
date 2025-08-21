#include "raylib.h"
#include "sound-engine/engine.h"

int main()
{
	const int screenWidth = 800;
	const int screenHeight = 450;

	InitWindow(screenWidth, screenHeight, "raylib [models] example - first person maze");
	InitAudioDevice();

	auto engine = std::make_unique<se::Engine>();
	engine->Initialize();

	// Define the camera to look into our 3d world
	Camera camera = { 0 };
	camera.position = Vector3{ 0.2f, 0.4f, 0.2f };    // Camera position
	camera.target = Vector3{ 0.185f, 0.4f, 0.0f };    // Camera looking at point
	camera.up = Vector3{ 0.0f, 1.0f, 0.0f };          // Camera up vector (rotation towards target)
	camera.fovy = 45.0f;                                // Camera field-of-view Y
	camera.projection = CAMERA_PERSPECTIVE;             // Camera projection type

	Image imMap = LoadImage("resources/cubicmap.png");      // Load cubicmap image (RAM)
	Texture2D cubicmap = LoadTextureFromImage(imMap);       // Convert image to texture to display (VRAM)
	Mesh mesh = GenMeshCubicmap(imMap, Vector3{ 1.0f, 1.0f, 1.0f });
	Model model = LoadModelFromMesh(mesh);

	// NOTE: By default each cube is mapped to one part of texture atlas
	Texture2D texture = LoadTexture("resources/cubicmap_atlas.png");    // Load map texture
	model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;    // Set map diffuse texture

	// Get map image data to be used for collision detection
	Color* mapPixels = LoadImageColors(imMap);
	UnloadImage(imMap);             // Unload image from RAM

	Vector3 mapPosition = { -16.0f, 0.0f, -8.0f };  // Set model position

//	Sound sound = LoadSoundFromWave(wave); // Load maze music sound
//	PlaySound(sound);

	auto auralizer = engine->GetAuralizer();
	Wave wave = LoadWave("resources/music.mp3"); // Load maze music wave

	// Let's test with a wave made up of just 1.0s to see if the auralizer works
	int waveSize = 44100 * 0.1; // 0.1 seconds of audio at 44100 Hz
	float* testWave = new float[waveSize];
	for (int i = 0; i < waveSize; i++)
	{
		testWave[i] = 0.0f; // Fill with 1.0s
	}

	testWave[0] = 1.0f; // Set the first sample to 1.0 to simulate a sound

	auralizer->Init();
	auto transformedWave = auralizer->Process(testWave, waveSize);

	// Convert wave to mono and keep only the first 10 seconds
	if (wave.channels > 1)
	{
		float* originalData = (float*)wave.data;

		wave.frameCount = (int)(wave.sampleRate * 10); // Limit to 10 seconds

		wave.data = RL_MALLOC(wave.frameCount * sizeof(float));
		for (int i = 0; i < wave.frameCount; i++)
		{
			((float*)wave.data)[i] = originalData[i * wave.channels]; // Take only the first channel
		}

		wave.channels = 1;

		RL_FREE(originalData);
	}

//	float testSamples[] = { 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };
//	auto newSamples = auralizer->TestInit(testSamples, 4);
	auto newSamples = auralizer->Process((float*)wave.data, wave.frameCount);
	Wave newWave = { 0 };
	newWave.sampleRate = wave.sampleRate;
	newWave.sampleSize = 32; // 32-bit float
	newWave.channels = 1; // Mono
	newWave.frameCount = (int)newSamples.size();
	newWave.data = RL_MALLOC(newWave.frameCount * sizeof(float));
	for (int i = 0; i < newWave.frameCount; i++)
	{
		if (newSamples[i] != newSamples[i]) // Check if the sample is NaN
		{
			newSamples[i] = 0; // Replace NaN with a valid value
		}
		((float*)newWave.data)[i] = newSamples[i];
	}

//	SetAudioStreamBufferSizeDefault(512);
//	AudioStream stream = LoadAudioStream(44100, 32, 1); // Load audio stream for music playback

	DisableCursor();                // Limit cursor to relative movement inside the window

	SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
	//--------------------------------------------------------------------------------------

	Sound sound = LoadSoundFromWave(newWave); // Load maze music sound
	PlaySound(sound);

	// Main game loop
	while (!WindowShouldClose())    // Detect window close button or ESC key
	{
		// Update
		//----------------------------------------------------------------------------------
		Vector3 oldCamPos = camera.position;    // Store old camera position

		UpdateCamera(&camera, CAMERA_FIRST_PERSON);

		// Check player collision (we simplify to 2D collision detection)
		Vector2 playerPos = { camera.position.x, camera.position.z };
		float playerRadius = 0.1f;  // Collision radius (player is modelled as a cilinder for collision)

		int playerCellX = (int)(playerPos.x - mapPosition.x + 0.5f);
		int playerCellY = (int)(playerPos.y - mapPosition.z + 0.5f);

		// Out-of-limits security check
		if (playerCellX < 0) playerCellX = 0;
		else if (playerCellX >= cubicmap.width) playerCellX = cubicmap.width - 1;

		if (playerCellY < 0) playerCellY = 0;
		else if (playerCellY >= cubicmap.height) playerCellY = cubicmap.height - 1;

		// Check map collisions using image data and player position
		// TODO: Improvement: Just check player surrounding cells for collision
		for (int y = 0; y < cubicmap.height; y++)
		{
			for (int x = 0; x < cubicmap.width; x++)
			{
				if ((mapPixels[y * cubicmap.width + x].r == 255) &&       // Collision: white pixel, only check R channel
					(CheckCollisionCircleRec(playerPos, playerRadius,
						Rectangle{ mapPosition.x - 0.5f + x * 1.0f, mapPosition.z - 0.5f + y * 1.0f, 1.0f, 1.0f })))
				{
					// Collision detected, reset camera position
					camera.position = oldCamPos;
				}
			}
		}
		//----------------------------------------------------------------------------------

		// Draw
		//----------------------------------------------------------------------------------
		BeginDrawing();

		ClearBackground(RAYWHITE);

		BeginMode3D(camera);
		DrawModel(model, mapPosition, 1.0f, WHITE);                     // Draw maze map
		EndMode3D();

		DrawTextureEx(cubicmap, Vector2{ GetScreenWidth() - cubicmap.width * 4.0f - 20, 20.0f }, 0.0f, 4.0f, WHITE);
		DrawRectangleLines(GetScreenWidth() - cubicmap.width * 4 - 20, 20, cubicmap.width * 4, cubicmap.height * 4, GREEN);

		// Draw player position radar
		DrawRectangle(GetScreenWidth() - cubicmap.width * 4 - 20 + playerCellX * 4, 20 + playerCellY * 4, 4, 4, RED);

		DrawFPS(10, 10);

		EndDrawing();
		//----------------------------------------------------------------------------------
	}

	// De-Initialization
	//--------------------------------------------------------------------------------------
	UnloadImageColors(mapPixels);   // Unload color array

	UnloadTexture(cubicmap);        // Unload cubicmap texture
	UnloadTexture(texture);         // Unload map texture
	UnloadModel(model);             // Unload map model

	engine->Shutdown();            // Shutdown sound engine

	CloseWindow();                  // Close window and OpenGL context
	//--------------------------------------------------------------------------------------

	return 0;
}