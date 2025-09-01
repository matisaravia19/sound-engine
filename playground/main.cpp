#include "raylib.h"
#include "sound-engine/engine.h"
#include "sound-engine/impulse.h"

Wave GetWave();
Wave GetNewWave(const Wave& wave, std::vector<float>& newSamples);

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

	Wave wave = GetWave();

	// Build se::Scene from the generated Raylib mesh and upload to GPU
	// Extract vertex positions and indices, and set transform to mapPosition
	se::Scene scene;
	{
		se::Mesh sMesh;
		sMesh.vertices.resize(mesh.vertexCount);
		// Raylib Mesh vertices are float array of size vertexCount*3: [x0,y0,z0, x1,y1,z1, ...]
		for (int i = 0; i < mesh.vertexCount; i++)
		{
			sMesh.vertices[i].position[0] = mesh.vertices[i * 3 + 0];
			sMesh.vertices[i].position[1] = mesh.vertices[i * 3 + 1];
			sMesh.vertices[i].position[2] = mesh.vertices[i * 3 + 2];
		}

		// Indices may be 0..vertexCount-1 if none provided; Raylib indices are unsigned short (triangle list)
		if (mesh.indices != nullptr && mesh.triangleCount > 0)
		{
			sMesh.indices.reserve(mesh.triangleCount * 3);
			for (int i = 0; i < mesh.triangleCount * 3; ++i)
			{
				sMesh.indices.push_back(static_cast<unsigned int>(mesh.indices[i]));
			}
		}
		else
		{
			// Fallback: generate a linear index buffer
			sMesh.indices.reserve(mesh.vertexCount);
			for (int i = 0; i < mesh.vertexCount; ++i) sMesh.indices.push_back(static_cast<unsigned int>(i));
		}

		// Set transform as translation to match DrawModel(model, mapPosition, 1.0f)
		for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) sMesh.transform[r][c] = 0.0f;
		sMesh.transform[0][0] = 1.0f;
		sMesh.transform[1][1] = 1.0f;
		sMesh.transform[2][2] = 1.0f;
		sMesh.transform[3][3] = 1.0f;
		sMesh.transform[0][3] = mapPosition.x;
		sMesh.transform[1][3] = mapPosition.y;
		sMesh.transform[2][3] = mapPosition.z;

		scene.meshes.push_back(std::move(sMesh));
	}

	auto ir = engine->GetImpulseResponseMapper();
	ir->UploadScene(scene);
	ir->InitializeRaytracing();

	auto auralizer = engine->GetAuralizer();
	auralizer->Init();

	auto newSamples = auralizer->Process((float*)wave.data, wave.frameCount);
	Wave newWave = GetNewWave(wave, newSamples);

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
Wave GetNewWave(const Wave& wave, std::vector<float>& newSamples)
{
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
	return newWave;
}

Wave GetWave()
{
	Wave wave = LoadWave("resources/music.mp3"); // Load maze music wave
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

	return wave;
}
