#include <SFML/Graphics.hpp>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <queue>
#include <fstream>
#include <sstream>
#include <thread>
#include <iomanip>
#include <utility>
#include <future>
#include <random>
#include <array>

#include "ThreadPool.h"

//Main.cpp

#define M_PI 3.14159265358979323846

const int WIDTH = 1280;
const int HEIGHT = 720;
const bool FULLSCREEN = false;

const float GRAVITY = 0.1f; // gravity
const float MAX_VELOCITY = 3.0f; // terminal velocity

bool paused = false;

const int cellSize = 2;
const int gridWidth = WIDTH / cellSize;
const int gridHeight = HEIGHT / cellSize;

struct Particle {
    uint8_t type;
	sf::Color color;
	bool active;
	bool updated;
};

int numColumns = std::thread::hardware_concurrency();
int columnWidth = gridWidth / numColumns;

int particleCount = 0;

sf::Color baseSandColor = { 253, 238, 115 };

//Main Simulation Grid
Particle grid[gridWidth][gridHeight];
//Active Cells
bool gridChanged[gridWidth][gridHeight] = { false };

//List of Materials
enum Materials {
    AIR,
    SAND,
    STONE,
    WATER,
    MUD,
    POISON,
    NUM_PARTICLE_TYPES
};

std::vector<sf::Color> sandyColors = {
	sf::Color(255, 223, 128), // Bright yellowish sand
	sf::Color(250, 210, 140), // Light golden
	sf::Color(255, 230, 150), // Soft yellow
	sf::Color(245, 200, 120), // Warm tan
	sf::Color(255, 218, 130)  // Sandy yellow
};

std::vector<sf::Color> stoneColors = {
	sf::Color(128, 128, 128), // Medium gray
	sf::Color(105, 105, 105), // Dim gray
	sf::Color(169, 169, 169), // Light gray
	sf::Color(96, 96, 96),    // Dark stone gray
	sf::Color(112, 128, 144)  // Slate gray
};

sf::Color setSandColor(Particle particle) {
	static std::mt19937 rng{ std::random_device{}() }; // Random number generator
	std::uniform_int_distribution<std::size_t> dist(0, sandyColors.size() - 1);

	// Select a random sandy color from the list
	return sf::Color(sandyColors[dist(rng)]);
}

sf::Color setStoneColor(Particle particle) {
	static std::mt19937 rng{ std::random_device{}() }; // Random number generator
	std::uniform_int_distribution<std::size_t> dist(0, stoneColors.size() - 1);

	// Select a random water color from the list
	return sf::Color(stoneColors[dist(rng)]);
}

Particle createSandParticle() {
	Particle particle;
	particle.type = SAND;
	particle.color = setSandColor(particle);
	return particle;
}

Particle createStoneParticle() {
	Particle particle;
	particle.type = STONE;
	particle.color = setStoneColor(particle);
	return particle;
}

void drawCircleOutline(sf::RenderWindow& window, int centerX, int centerY, int radius) {
	const int numPoints = static_cast<int>(2 * M_PI * radius); // Calculate the number of points based on the radius
	sf::VertexArray circle(sf::LinesStrip, numPoints + 1); // +1 to close the circle

	for (int i = 0; i < numPoints; ++i) {
		float angle = static_cast<float>(i) / numPoints * 2 * M_PI; // Angle for the current point
		int x = static_cast<int>(centerX + radius * cos(angle));
		int y = static_cast<int>(centerY + radius * sin(angle));

		circle[i].position = sf::Vector2f(static_cast<float>(x), static_cast<float>(y)); // Set the position of the point
		circle[i].color = sf::Color::White; // Set the color of the point (white in this case)
	}

	// Closing the circle by setting the first point at the end
	circle[numPoints].position = circle[0].position;
	circle[numPoints].color = sf::Color::White;

	window.draw(circle); // Draw the circle outline
}

void checkClick(sf::Mouse::Button buttonCode, int x, int y, int blobSize, int cellSize, Materials MAT, const sf::RenderWindow& window) {
	if (sf::Mouse::isButtonPressed(buttonCode)) {

		sf::Vector2i mousePos = sf::Mouse::getPosition(window);

		int centerX = x / cellSize;
		int centerY = y / cellSize;
		for (int i = -blobSize; i <= blobSize; i++) {
			for (int j = -blobSize; j <= blobSize; j++) {
				if (i * i + j * j <= blobSize * blobSize) {
					int newX = centerX + i;
					int newY = centerY + j;

					if (newX >= 0 && newX < gridWidth && newY >= 0 && newY < gridHeight) {
						if (MAT == STONE || blobSize <= 2) {
							grid[newX][newY].type = MAT;
						}
						if (MAT == STONE) {
							grid[newX][newY] = createStoneParticle();
						}
						if (rand() % 40 == 0 && MAT == SAND && blobSize > 2) {
							grid[newX][newY] = createSandParticle();
						}
						if (MAT == SAND && blobSize <= 2) {
							grid[newX][newY] = createSandParticle();
						}
						else if (rand() % 14 == 0 && blobSize > 2 && MAT != SAND) {
							grid[newX][newY].type = MAT;
						}

					}
				}
			}
		}

	}
}

bool isSurroundedByStableCells(int i, int j) {
	if (grid[i][j + 1].type != AIR) return true;
	if (i > 0 && grid[i - 1][j].type != AIR) return true;
	if (i < gridWidth - 1 && grid[i + 1][j].type != AIR) return true;
	return false;
}

void synchronizeBoundaries(int col, int columnWidth) {
	int startColumn = col * columnWidth;
	int endColumn = (col == numColumns - 1) ? gridWidth : startColumn + columnWidth;

	for (int j = gridHeight - 2; j >= 0; j--) {
		if (startColumn < gridWidth - 1) {
			if (grid[startColumn + 1][j].type == AIR && grid[startColumn][j].type != AIR) {
				grid[startColumn + 1][j].type = grid[startColumn][j].type;
				grid[startColumn][j].type = AIR;
			}
		}
	}

}

void drawColumnLines(sf::RenderWindow& window, int screenWidth, int screenHeight, int numColumns) {
	int columnWidth = screenWidth / numColumns;

	// Create a VertexArray to store all the lines
	sf::VertexArray lines(sf::Lines);

	// Set color to red and create vertical lines
	for (int i = 1; i < numColumns; i++) {
		int x = i * columnWidth;

		// Add the two vertices for the line (start and end point)
		lines.append(sf::Vertex(sf::Vector2f(x, 0), sf::Color::Red));
		lines.append(sf::Vertex(sf::Vector2f(x, screenHeight), sf::Color::Red));
	}

	// Draw all lines at once
	window.draw(lines);
}

int countActiveParticles(const Particle grid[gridWidth][gridHeight]) {
	int count = 0;
	for (int i = 0; i < gridWidth; i++) {
		for (int j = 0; j < gridHeight; j++) {
			if (grid[i][j].type != AIR) { // Count non-air particles
				count++;
			}
		}
	}
	return count;
}

std::string toString(int fps) {
	return "FPS: " + std::to_string(fps);
}

std::vector<std::pair<int, int>> activeParticles;

void simulateColumn(int startColumn, int endColumn, bool gridChanged[gridWidth][gridHeight]) {
	if (!paused) {
		for (int i = startColumn; i < endColumn; i++) {
			for (int j = gridHeight - 2; j >= 0; j--) {
				int currentType = grid[i][j].type;

				// Skip inactive cells or cells that are air (nothing to simulate)
				if (currentType == AIR) continue;

				int belowType = grid[i][j + 1].type;

				// Sand logic
				if (currentType == SAND) {
					if (belowType == AIR) {
						// Move sand down
						grid[i][j + 1].type = SAND;
						grid[i][j + 1].color = grid[i][j].color;
						grid[i][j].type = AIR;
						gridChanged[i][j] = true;         // Mark current cell as changed
						gridChanged[i][j + 1] = true;     // Mark new position as changed
					}
					else if (i > 0 && grid[i - 1][j + 1].type == AIR) {
						// Move sand down-left
						grid[i - 1][j + 1].type = SAND;
						grid[i - 1][j + 1].color = grid[i][j].color;
						grid[i][j].type = AIR;
						gridChanged[i][j] = true;
						gridChanged[i - 1][j + 1] = true;
					}
					else if (i < gridWidth - 1 && grid[i + 1][j + 1].type == AIR) {
						// Move sand down-right
						grid[i + 1][j + 1].type = SAND;
						grid[i + 1][j + 1].color = grid[i][j].color;
						grid[i][j].type = AIR;
						gridChanged[i][j] = true;
						gridChanged[i + 1][j + 1] = true;
					}

				}

				// Water logic
				else if (currentType == WATER) {
					int direction = rand() % 2; // Randomly pick direction for diagonal movement

					if (belowType == AIR) {
						// Move water down
						grid[i][j + 1].type = WATER;
						grid[i][j + 1].color = grid[i][j].color;
						grid[i][j].type = AIR;
						gridChanged[i][j] = true;
						gridChanged[i][j + 1] = true;
					}
					else if (direction == 0 && i > 0 && grid[i - 1][j + 1].type == AIR) {
						// Move water down-left
						grid[i - 1][j + 1].type = WATER;
						grid[i - 1][j + 1].color = grid[i][j].color;
						grid[i][j].type = AIR;
						gridChanged[i][j] = true;
						gridChanged[i - 1][j + 1] = true;
					}
					else if (direction == 1 && i < gridWidth - 1 && grid[i + 1][j + 1].type == AIR) {
						// Move water down-right
						grid[i + 1][j + 1].type = WATER;
						grid[i + 1][j + 1].color = grid[i][j].color;
						grid[i][j].type = AIR;
						gridChanged[i][j] = true;
						gridChanged[i + 1][j + 1] = true;
					}
					else if (direction == 0 && i > 0 && grid[i - 1][j].type == AIR) {
						// Spread water left
						grid[i - 1][j].type = WATER;
						grid[i - 1][j].color = grid[i][j].color;
						grid[i][j].type = AIR;
						gridChanged[i][j] = true;
						gridChanged[i - 1][j] = true;
					}
					else if (direction == 1 && i < gridWidth - 1 && grid[i + 1][j].type == AIR) {
						// Spread water right
						grid[i + 1][j].type = WATER;
						grid[i + 1][j].color = grid[i][j].color;
						grid[i][j].type = AIR;
						gridChanged[i][j] = true;
						gridChanged[i + 1][j] = true;
					}
				}

				// Poison logic (same logic, add gridChanged updates)
				else if (currentType == POISON) {
					if (j + 1 < gridHeight && grid[i][j + 1].type == AIR) {
						grid[i][j + 1].type = POISON;
						grid[i][j].type = AIR;
						gridChanged[i][j] = true;
						gridChanged[i][j + 1] = true;
					}
					else if (j + 1 < gridHeight && grid[i][j + 1].type != AIR) {
						grid[i][j + 1].type = AIR;
						gridChanged[i][j + 1] = true;
						gridChanged[i][j] = true;
					}
					else {
						int direction = rand() % 2;
						if (direction == 0 && i > 0 && j + 1 < gridHeight && grid[i - 1][j + 1].type == AIR) {
							grid[i - 1][j + 1].type = POISON;
							grid[i][j].type = AIR;
							gridChanged[i][j] = true;
							gridChanged[i - 1][j + 1] = true;
						}
						else if (direction == 1 && i < gridWidth - 1 && j + 1 < gridHeight && grid[i + 1][j + 1].type == AIR) {
							grid[i + 1][j + 1].type = POISON;
							grid[i][j].type = AIR;
							gridChanged[i][j] = true;
							gridChanged[i + 1][j + 1] = true;
						}
					}
				}
			}
		}
	}
}



sf::Color sandColor = { 253, 238, 115, 255 };
sf::Color stoneColor = { 74, 74, 74, 255 };
sf::Color waterColor = { 15, 94, 156, 255 };
sf::Color mudColor = { 130, 73, 23, 255 };
sf::Color poisonColor = { 0, 255, 8, 255 };
sf::VertexArray quads(sf::Quads, gridWidth* gridHeight * 4); // Create once, resize only if needed

sf::Texture texture;
sf::Sprite sprite;
std::vector<sf::Color> pixelBuffer(gridWidth* cellSize* gridHeight* cellSize, sf::Color::Black); // Pre-allocate pixel buffer



void processChunk(int startX, int startY, int endX, int endY, std::vector<sf::Color>& pixelBuffer, const Particle grid[gridWidth][gridHeight]) {
	for (int i = startX; i < endX; i++) {
		for (int j = startY; j < endY; j++) {
			Particle particle = grid[i][j];

			// Skip if it's air
			if (particle.type == AIR) {
				continue;
			}


			sf::Color color;
			switch (particle.type) {
			case SAND:
				color = particle.color;
				break;
			case STONE:
				color = particle.color;
				break;
			case WATER:
				color = waterColor;
				break;
			case MUD:
				color = mudColor;
				break;
			case POISON:
				color = poisonColor;
				break;
			default:
				continue; // Skip if not a recognized type
			}

			for (int x = 0; x < cellSize; x++) {
				for (int y = 0; y < cellSize; y++) {
					int pixelX = i * cellSize + x;
					int pixelY = j * cellSize + y;

					// Ensure we do not go out of bounds
					if (pixelX < gridWidth * cellSize && pixelY < gridHeight * cellSize) {
						pixelBuffer[pixelY * (gridWidth * cellSize) + pixelX] = color;
					}
				}
			}
		}
	}
}

void renderGrid(sf::RenderWindow& window, const Particle grid[gridWidth][gridHeight]) {
	std::fill(pixelBuffer.begin(), pixelBuffer.end(), sf::Color(25,25,25));

	// Define thread count
	const int threadCount = 4; // Adjust based on your CPU
	std::vector<std::future<void>> futures;

	int chunkHeight = gridHeight / threadCount;

	// Create threads to process chunks of the grid
	for (int t = 0; t < threadCount; t++) {
		int startY = t * chunkHeight;
		int endY = (t == threadCount - 1) ? gridHeight : (t + 1) * chunkHeight;
		futures.push_back(std::async(std::launch::async, processChunk, 0, startY, gridWidth, endY, std::ref(pixelBuffer), grid));
	}

	// Wait for all threads to complete
	for (auto& fut : futures) {
		fut.get();
	}

	// Create/update texture
	texture.create(gridWidth * cellSize, gridHeight * cellSize);
	texture.update(reinterpret_cast<const sf::Uint8*>(pixelBuffer.data()));
	sprite.setTexture(texture);
	window.draw(sprite);
}

int main()
{
    sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "Power Game");

	ThreadPool pool(numColumns);

	sf::Clock clock;
	sf::Time elapsed = clock.getElapsedTime();
	uint32_t startTime = elapsed.asMilliseconds();
	int frameCount = 0;
	int fps = 0;

	// Load debugging font
	sf::Font font;
	if (!font.loadFromFile("C:/Users/clearly/source/repos/SFML-PowderGame/Fonts/Golden Age.ttf")) {
		std::cerr << "Failed to load font from: C:/Users/clearly/source/repos/SFML-PowderGame/Fonts/Golden Age.ttf" << std::endl;
	}

	// Create the text
	sf::Text fpsText;
	fpsText.setFont(font);  // Set the font
	fpsText.setString("FPS: ");  // Set the text
	fpsText.setCharacterSize(24);  // Set the font size
	fpsText.setFillColor(sf::Color::White);  // Set the text color (white)
	fpsText.setPosition(sf::Vector2f(10, 10));

	sf::Text particleText;
	particleText.setFont(font);  // Set the font
	particleText.setString("Particle Count: ");  // Set the text
	particleText.setCharacterSize(24);  // Set the font size
	particleText.setFillColor(sf::Color::White);  // Set the text color (white)
	particleText.setPosition(sf::Vector2f(10, 30));

	//Set all Grid spaces to AIR
	for (int i = 0; i < gridWidth; i++) {
		for (int j = 0; j < gridHeight; j++) {
			grid[i][j].type = AIR;
			gridChanged[i][j] = false;
		}
	}

	bool blobEvent = false;
	int blobSize = 5;

	std::cout << "Starting Game" << std::endl;

	std::vector<std::thread> threads;

    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();
			if (event.type == sf::Event::MouseWheelScrolled) {
				if (event.mouseWheelScroll.delta > 0) {
					blobSize += 1;
					blobEvent = true;
				}
				else if (event.mouseWheelScroll.delta < 0 && blobSize >= 1) {
					blobSize -= 1;
					blobEvent = true;
				}
			}
			if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Space) {
					std::cout << "Paused Game" << std::endl;
					paused = !paused;
			}
        }

		

		particleCount = countActiveParticles(grid);

		int x, y;
		x = sf::Mouse::getPosition(window).x;
		y = sf::Mouse::getPosition(window).y;

		checkClick(sf::Mouse::Left, x, y, blobSize, cellSize, SAND, window);
		checkClick(sf::Mouse::Right, x, y, blobSize, cellSize, STONE, window);
		checkClick(sf::Mouse::Middle, x, y, blobSize, cellSize, WATER, window);
		checkClick(sf::Mouse::XButton1, x, y, blobSize, cellSize, POISON, window);

		int numColumnsPerTask = 1;
		for (int i = 0; i < numColumns; i += numColumnsPerTask) {
			// Calculate the start and end of the column range
			int start = i * columnWidth;
			int end = std::min((i + numColumnsPerTask) * columnWidth, gridWidth);

			pool.enqueue([start, end]() {
				simulateColumn(start, end, gridChanged);
				});
		}

		synchronizeBoundaries(numColumns, columnWidth);

        window.clear(sf::Color(20,20,20,255));

        //Rendering loop
		renderGrid(window, grid);

		drawColumnLines(window, WIDTH, HEIGHT, numColumns);

		if (blobEvent) {
			drawCircleOutline(window, x, y, blobSize * cellSize);
		}

		//Draw Text

		frameCount++;
		uint32_t currentTime = clock.getElapsedTime().asMilliseconds();
		if (currentTime - startTime >= 100) {
			fps = frameCount;
			frameCount = 0;
			startTime = currentTime;
		}

		fpsText.setString(toString(fps));
		particleText.setString("Particle Count: " + std::to_string(particleCount));

		window.draw(fpsText);
		window.draw(particleText);

        window.display();
    }

    return 0;
}