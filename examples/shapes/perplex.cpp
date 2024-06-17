#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <math.h>
#include <vector>
#include <utility>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm> // for std::shuffle
#include <random> // for std::random_device and std::mt19937
#include <numeric>

using namespace std;

vector<int> permutation;
vector<Color> colors;
vector<vector<int>> displayboard;
vector<vector<int>> workboard;
vector<vector<int>> challengeboard;
vector<vector<vector<pair<int, int>>>> allPieces;
vector<pair<int, int>> neighbours;
int solutions = 0;
mutex mtx;
int maxSolutions = 1;
atomic<bool> ready(false);
int maxRemovedPieces = 0;

// Create a random number generator
random_device rd;
mt19937 g(rd());

void clearBoards()
{
	workboard.resize(7);
	displayboard.resize(7);
	for (int i = 0; i < 7; i++)
	{
		workboard[i].resize(7, 0);
		displayboard[i].resize(7, 0);

		for (int j = 0; j < 7; j++)
		{
			workboard[i][j] = 0;
		}
	}
}

bool canPlacePiece(vector<vector<int>>& grid, const vector<pair<int, int>>& piece, int startRow, int startCol)
{
	for (const auto& coords : piece)
	{
		int row = startRow + coords.first;
		int col = startCol + coords.second;

		if (row + col < 3 || row + col > 9)
			return false;

		if (row < 0 || row >= grid.size() || col < 0 || col >= grid[0].size() || grid[row][col] != 0)
		{
			return false;
		}
	}
	return true;
}

void rotatePiece(vector<pair<int, int>>& piece)
{
	int minX = FLT_MAX;
	int minY = FLT_MAX;

	for (auto& coords : piece)
	{
		int x = coords.first;
		int y = coords.second;

		coords.first = y;
		coords.second = -(x + y);

		minX = min(coords.first, minX);
		minY = min(coords.second, minY);
	}

	for (auto& coords : piece)
	{
		coords.first -= minX;
		coords.second -= minY;
	}
}

void mirrorPiece(vector<pair<int, int>>& piece)
{
	int minX = FLT_MAX;
	int minY = FLT_MAX;

	for (auto& coords : piece)
	{
		int x = coords.first;
		int y = coords.second;

		coords.first = x + y;
		coords.second = -y;

		x = coords.first;
		y = coords.second;

		coords.first = -y;
		coords.second = x + y;

		minX = min(coords.first, minX);
		minY = min(coords.second, minY);
	}

	for (auto& coords : piece)
	{
		coords.first -= minX;
		coords.second -= minY;
	}
}

void generateVariants(vector<vector<vector<pair<int, int>>>>& pieces, vector<pair<int, int>> piece, bool isSymmetric)
{
	vector<vector<pair<int, int>>> variants;

	for (int symm = 0; symm < 2; symm++)
	{
		if (symm == 1)
		{
			if (isSymmetric)
			{
				pieces.push_back(variants);
				return;
			}

			mirrorPiece(piece);
		}

		for (int rot = 0; rot < 3; rot++)
		{
			variants.push_back(piece);
			rotatePiece(piece);
		}
	}

	pieces.push_back(variants);
}

void placePiece(vector<vector<int>>& grid, const vector<pair<int, int>>& piece, int startRow, int startCol, int pieceId)
{
	for (const auto& coords : piece)
	{
		grid[startRow + coords.first][startCol + coords.second] = pieceId;
	}
}

bool solvePuzzle(vector<vector<int>>& grid, vector<vector<vector<pair<int, int>>>>& pieces, int pieceIndex, int startX, int startY, bool stop = true)
{
	if (pieceIndex <= 8 && pieceIndex > 0)
	{
		{
			//lock_guard<mutex> lock(mtx);
			//displayboard = workboard;
		}

		for (int row = 0; row < grid.size(); ++row)
		{
			for (int col = 0; col < grid[0].size(); ++col)
			{
				if (row + col >= 3 && row + col <= 9 && grid[row][col] == 0)
				{
					int validNeighbours = 0;
					for (int n = 0; n < neighbours.size(); n++)
					{
						int x = col + neighbours[n].first;
						int y = row + neighbours[n].second;

						if (x >= 0 && y >= 0 && y < grid.size() && x < grid[y].size() && x + y >= 3 && x + y <= 9 && grid[y][x] == 0)
							validNeighbours++;
					}

					if (validNeighbours == 0)
						return false;
				}
			}
		}
	}

	if (pieceIndex == pieces.size())
	{
		solutions++;
		displayboard = workboard;
		return true;
	}

	for (int row = startY; row < grid.size(); ++row)
	{
		for (int col = startX; col < grid[0].size(); ++col)
		{
			vector<vector<pair<int, int>>>& variants = pieces[pieceIndex];

			for (int var = 0; var < variants.size(); var++)
			{
				if (canPlacePiece(grid, variants[var], row, col))
				{
					placePiece(grid, variants[var], row, col, pieceIndex + 1); // Place the piece.

					if (solvePuzzle(grid, pieces, pieceIndex + 1, 0, 0, stop))
					{
						if(stop || solutions > maxSolutions)
							return true;
					}
					placePiece(grid, variants[var], row, col, 0); // Remove the piece.
				}
			}
		}
	}

	return false;
}

template <typename T>
void applyPermutation(vector<T>& vec, const vector<int>& permutation) {
	vector<T> temp(vec.size());
	for (size_t i = 0; i < permutation.size(); ++i) {
		temp[i] = vec[permutation[i]];
	}
	vec = move(temp);
}

void removePiece(vector<vector<int>>& grid, int pieceIndex, vector<pair<int, int>>& piecePos)
{
	for (int row = 0; row < grid.size(); ++row)
	{
		for (int col = 0; col < grid[0].size(); ++col)
		{
			if (grid[row][col] == pieceIndex)
			{
				grid[row][col] = 0;
				piecePos.push_back(make_pair(row, col));
			}
		}
	}
}

void workerThread()
{
	while (true)
	{
		clearBoards();
		// Shuffle the permutation
		shuffle(permutation.begin(), permutation.end(), g);

		// Shuffle the main pieces vector
		applyPermutation(allPieces, permutation);
		applyPermutation(colors, permutation);

		// Shuffle each piece's variants
		for (auto& pieceVariants : allPieces) {
			shuffle(pieceVariants.begin(), pieceVariants.end(), g);
		}
		solvePuzzle(workboard, allPieces, 0, GetRandomValue(0, 6), GetRandomValue(0, 6));

		vector<vector<vector<pair<int, int>>>> removedPieces;
		vector<int> removedPiecesIds;
		challengeboard = workboard;
		shuffle(permutation.begin(), permutation.end(), g);

		for (int i = 0; i < 10; i++)
		{
			for (int perm = 0; perm < permutation.size(); perm++)
			{
				bool skip = false;
				for (int j = 0; j < removedPiecesIds.size(); j++)
					if (removedPiecesIds[j] == permutation[perm] + 1)
					{
						skip = true;
						break;
					}

				if (skip)
					continue;

				vector<pair<int, int>> piecePos;
				removePiece(challengeboard, permutation[perm] + 1, piecePos);
				removedPieces.push_back(allPieces[permutation[perm]]);
				removedPiecesIds.push_back(permutation[perm] + 1);

				solutions = 0;
				workboard = challengeboard;

				solvePuzzle(workboard, removedPieces, 0, 0, 0, false);

				if (solutions > maxSolutions)
				{
					for (int i = 0; i < piecePos.size(); i++)
					{
						challengeboard[piecePos[i].first][piecePos[i].second] = permutation[perm] + 1;
					}
					removedPieces.erase(removedPieces.end() - 1);
					removedPiecesIds.erase(removedPiecesIds.end() - 1);
				}
			}
		}

		//if (maxRemovedPieces < removedPiecesIds.size())
		{
			displayboard = challengeboard;
			maxRemovedPieces = removedPiecesIds.size();
		}

		while (!ready.load())
		{
			std::this_thread::yield(); // Efficiently wait without busy-waiting
		}

		ready.store(false);
	}
}

int main(void)
{
	// Initialization
	const int screenWidth = 800;
	const int screenHeight = 600;

	InitWindow(screenWidth, screenHeight, "Perplex");

	// Hexagon parameters
	float radius = 40.0f; // Radius of each hexagon
	int sides = 6;        // Number of sides (6 for hexagon)
	float rotation = 0.0f;

	// Calculate the horizontal and vertical distance between hexagon centers
	float horizDist = 3.0f / 2.0f * radius; // 1.5 times the radius
	float vertDist = sqrt(3.0f) * radius;   // sqrt(3) times the radius

	SetTargetFPS(60);     // Set our game to run at 60 frames-per-second

	clearBoards();

	generateVariants(allPieces, { {0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1} }, true);
	colors.push_back(CLITERAL(Color) { 237, 227, 33, 255 });

	generateVariants(allPieces, { {0, 0}, {1, 0}, {2, 0} }, true);
	colors.push_back(ORANGE);

	generateVariants(allPieces, { {0, 0}, {1, 0}, {1, 1} }, false);
	colors.push_back(DARKBLUE);

	generateVariants(allPieces, { {0, 1}, {1, 0}, {1, 1} }, true);
	colors.push_back(CLITERAL(Color) { 0, 139, 139, 255 });

	generateVariants(allPieces, { {0, 0}, {1, 0}, {0, 1} }, true);
	colors.push_back(PINK);

	generateVariants(allPieces, { {0, 1}, {1, 0}, {1, 1}, {1, 2} }, false);
	colors.push_back(DARKGREEN);

	generateVariants(allPieces, { {0, 0}, {0, 1}, {0, 2}, {1, 1} }, false);
	colors.push_back(CLITERAL(Color) { 43, 210, 126, 255 });

	generateVariants(allPieces, { {0, 0}, {0, 1}, {1, 0}, {1, 1} }, false);
	colors.push_back(DARKPURPLE);

	generateVariants(allPieces, { {0, 0}, {0, 1}, {0, 2}, {1, 2} }, false);
	colors.push_back(RED);

	generateVariants(allPieces, { {0, 0}, {0, 1}, {1, 1}, {1, 2} }, false);
	colors.push_back(CLITERAL(Color) { 75, 170, 200, 255 });

	neighbours = { {0, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, 0}, {-1, 1} };

	permutation.resize(allPieces.size());
	iota(permutation.begin(), permutation.end(), 0); // Fill with 0, 1, ..., size-1

	// Shuffle the permutation
	shuffle(permutation.begin(), permutation.end(), g);

	// Shuffle the main pieces vector
	applyPermutation(allPieces, permutation);
	applyPermutation(colors, permutation);

	// Shuffle each piece's variants
	for (auto& pieceVariants : allPieces) {
		shuffle(pieceVariants.begin(), pieceVariants.end(), g);
	}

	thread worker(workerThread);

	// Main game loop
	while (!WindowShouldClose()) // Detect window close button or ESC key
	{
		// Update

		// Draw
		BeginDrawing();

		ClearBackground(DARKGRAY);


		if (GuiButton(Rectangle({ 10, 10, 50, 20 }), "Retry"))
		{
			ready.store(true);
		}
		DrawText(TextFormat("Solutions found: %d", solutions), 10, 30, 20, BLACK);

		// Draw the hexagon grid
		for (int row = 0; row <= 6; row++)
		{
			for (int col = 0; col <= 6; col++)
			{
				int x = col - 3;
				int y = row - 3;

				// Offset every other row for hexagonal tiling
				float xOffset = x * horizDist + screenWidth / 2;
				float yOffset = screenHeight - (y * vertDist + x * vertDist / 2 + screenHeight / 2);

				Vector2 center = { xOffset, yOffset };

				if (x + y > 3 || x + y < -3)
				{
					//DrawPolyLines(center, sides, radius, rotation, RED);
					//DrawText(TextFormat("%d,%d", col, row), xOffset - horizDist / 4, yOffset - 7, 14, RED);
					continue;
				}

				Color color = GRAY;
				if (displayboard[col][row] != 0)
				{
					int value = displayboard[col][row] - 1;
					if (value >= 0 && value < colors.size())
						color = colors[value];
				}

				DrawPoly(center, sides, radius, rotation, color);
				DrawPolyLines(center, sides, radius, rotation, WHITE);
				DrawText(TextFormat("%d,%d", col, row), xOffset - horizDist / 5, yOffset - 7, 14, BLACK);
			}
		}

		EndDrawing();
	}

	worker.join();

	// De-Initialization
	CloseWindow(); // Close window and OpenGL context

	return 0;
}
