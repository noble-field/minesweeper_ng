#include <Siv3D.hpp>
#include <random>

#define CELL_COUNT_MINE -1

// close/open, flagged, count
struct Cell {
	bool opened = false;
	bool flagged = false;
	int count = 0;
};

// status(CLOSE_MINE...), *effectiveCount
struct InferredCell {
	int status;
};

struct GameState {
private:
	int closedPlainCount;
	int flagCount;
	bool mineSetup = false;
	bool bombed = false;
public:
	enum status {
		NOT_READY,
		PLAYING,
		GAMEOVER,
		GAMECLEAR
	};
	GameState(int ncells, int nmines) {
		closedPlainCount = ncells - nmines;
		flagCount = nmines;
	}
	void triggerMine() { bombed = true; }
	void setupMines() { mineSetup = true; }
	void openPlain() { closedPlainCount--; }
	void buildFlag() { flagCount--; }
	void removeFlag() { flagCount++; }
	void reset(int ncells, int nmines) {
		mineSetup = false;
		bombed = false;
		closedPlainCount = ncells - nmines;
		flagCount = nmines;
	}
	int getFlagCount() { return flagCount; }
	int getStatus() {
		if (!mineSetup) {
			return status::NOT_READY;
		}
		else if (bombed) {
			return status::GAMEOVER;
		}
		else if (closedPlainCount == 0) {
			return status::GAMECLEAR;
		}
		return status::PLAYING;
	}
};

const s3d::Point adjVec[] = {
	{ -1, -1 }, { 0, -1 }, { 1, -1 },
	{ -1,  0 }           , { 1,  0 },
	{ -1,  1 }, { 0,  1 }, { 1,  1 },
};

void clearField(s3d::Grid<Cell>& field)
{
	for (int r = 0; r < field.height(); r++) {
		for (int c = 0; c < field.width(); c++) {
			field[r][c].flagged = false;
			field[r][c].opened = false;
			field[r][c].count = 0;
		}
	}
}

void setupMines(s3d::Grid<Cell> &field, int nmines, s3d::Point avoidPos)
{
	const size_t ncells = field.width() * field.height();
	const size_t avoidIndex = avoidPos.y * field.width() + avoidPos.x;

	s3d::Array<int> fieldVec(ncells - 1, 0);
	for (int i = 0; i < nmines; i++)
		fieldVec[i] = CELL_COUNT_MINE;

	std::random_device seed_gen;
	std::mt19937 engine(seed_gen());
	std::shuffle(fieldVec.begin(), fieldVec.end(), engine);

	for (int r = 0; r < field.height(); r++) {
		for (int c = 0; c < field.width(); c++) {
			const size_t index = field.width() * r + c;

			if (index == avoidIndex) continue;
			field[r][c].count = fieldVec[index - (index > avoidIndex ? 1 : 0)];
		}
	}

	for (int r = 0; r < field.height(); r++) {
		for (int c = 0; c < field.width(); c++) {
			if (field[r][c].count == CELL_COUNT_MINE) continue;

			for (const auto& delta : adjVec) {
				const s3d::Point pos = s3d::Point{ c, r } + delta;
				if (field.inBounds(pos) && field[pos].count == CELL_COUNT_MINE) {
					field[r][c].count++;
				}
			}
		}
	}
}

// return true if a mine clicked
void openCell(GameState &gameState, s3d::Grid<Cell> &field, int r, int c)
{
	field[r][c].opened = true;
	if (field[r][c].flagged) {
		field[r][c].flagged = false;
		gameState.removeFlag();
	}
	if (field[r][c].count == CELL_COUNT_MINE) {
		gameState.triggerMine();
		return;
	}

	gameState.openPlain();
	if (field[r][c].count != 0) {
		return;
	}
	for (const auto& delta : adjVec) {
		const s3d::Point pos = s3d::Point{ c, r } + delta;
		if (field.inBounds(pos) && !field[pos].opened) {
			openCell(gameState, field, pos.y, pos.x);
		}
	}
}

void Main()
{
	const int screenWidth = 900;
	const int screenHeight = 800;
	const int infoBarHeight = 100;

	s3d::System::SetTerminationTriggers(s3d::UserAction::CloseButtonClicked);
	
	s3d::Window::SetTitle(U"Mine Sweeper Prototype");
	s3d::Window::Resize(screenWidth, screenHeight);
	s3d::Scene::SetBackground(s3d::Palette::White);

	// game config
	int nrows = 14;
	int ncols = 18;
	int nmines = 40;
	int size = 50;

	// game status
	GameState gameState(nrows * ncols, nmines);
	s3d::Grid<Cell> field(s3d::Size{ ncols, nrows });

	const s3d::Texture emojiFlag{ U"🚩"_emoji };
	const s3d::Font font{ s3d::FontMethod::MSDF, 48, s3d::Typeface::Bold };

	while (s3d::System::Update()) {

		// draw infomation bar
		s3d::Rect infoBarRect = s3d::Rect{ s3d::Arg::topLeft(0, 0), s3d::Size{screenWidth, infoBarHeight} };
		infoBarRect.draw(s3d::Palette::Black);
		emojiFlag.resized(size).draw(s3d::Arg::leftCenter(infoBarRect.center() - s3d::Point{ 2 * size, 0 }));
		font(U" × {}"_fmt(gameState.getFlagCount())).draw(size, s3d::Arg::leftCenter(infoBarRect.center() - s3d::Point{ size, 0 }));

		// draw field
		for (int r = 0; r < nrows; r++) {
			for (int c = 0; c < ncols; c++) {
				s3d::Rect cellRect(s3d::Arg::topLeft(c * size, r * size + infoBarHeight), size);
				s3d::ColorF cellColor = cellRect.mouseOver() ? s3d::Palette::Dodgerblue
					: ((r + c) & 1 ? s3d::Palette::Mediumblue : s3d::Palette::Darkblue);

				if (field[r][c].opened) {
					if (field[r][c].count == CELL_COUNT_MINE) {
						cellRect.draw(s3d::Palette::Red);
						cellRect.drawFrame(1.0, s3d::Palette::Darkred);
						s3d::Circle{ s3d::Arg::center(cellRect.center()), size / 4 }.draw(s3d::Palette::Darkorange);
					}
					else {
						s3d::ColorF openedCellColor = ((r + c) & 1 ? s3d::Palette::Lawngreen : s3d::Palette::Yellowgreen);
						cellRect.draw(openedCellColor);
						cellRect.drawFrame(1.0, s3d::Palette::Olivedrab);
					}

					if (field[r][c].count >= 1) {
						font(field[r][c].count).draw(0.8 * size, s3d::Arg::center(cellRect.center()), s3d::Palette::Black);
					}
				}
				else {
					cellRect.draw(cellColor);
					cellRect.drawFrame(1.0, s3d::Palette::Navy);
				}

				//if (field[r][c].count == CELL_COUNT_MINE) {
				//	s3d::Circle{ s3d::Arg::center(cellRect.center()), size / 4 }.draw(s3d::Palette::Darkorange);
				//}

				if (field[r][c].flagged) {
					emojiFlag.resized(0.7 * size).drawAt(cellRect.center());
				}
			}
		}

		// retry
		if ((gameState.getStatus() == GameState::status::GAMEOVER ||
			gameState.getStatus() == GameState::status::GAMECLEAR) && s3d::KeyR.down()) {
			clearField(field);
			gameState.reset(nrows * ncols, nmines);
		}

		// game over
		if (gameState.getStatus() == GameState::status::GAMEOVER) {
			s3d::Rect{ s3d::Arg::center(screenWidth / 2, screenHeight / 2), screenWidth, 3 * size }.draw(s3d::ColorF{ 0.0, 0.7 });
			font(U"GAME OVER").draw(s3d::TextStyle::Outline(0.25, s3d::ColorF{ 0.0 }), size * 2, s3d::Arg::center(screenWidth / 2, screenHeight / 2), s3d::Palette::Red);
			continue;
		}
		// game clear
		else if (gameState.getStatus() == GameState::status::GAMECLEAR) {
			s3d::Rect{ s3d::Arg::center(screenWidth / 2, screenHeight / 2), screenWidth, 3 * size }.draw(s3d::ColorF{ 0.0, 0.7 });
			font(U"★GAME CLEAR★").draw(s3d::TextStyle::Outline(0.25, s3d::ColorF{ 0.0 }), size * 2, s3d::Arg::center(screenWidth / 2, screenHeight / 2), s3d::Palette::Yellow);
			continue;
		}

		// player action
		for (int r = 0; r < nrows; r++) {
			for (int c = 0; c < ncols; c++) {
				if (field[r][c].opened) continue;

				s3d::Rect cellRect(s3d::Arg::topLeft(c * size, r * size + infoBarHeight), size);
				
				// open a cell
				if (cellRect.leftClicked() && !field[r][c].flagged) {
					if (gameState.getStatus() == GameState::status::NOT_READY) {
						gameState.setupMines();
						setupMines(field, nmines, s3d::Point{ c, r });
					}
					openCell(gameState, field, r, c);
				}
				// build a flag
				else if (cellRect.rightClicked()) {
					field[r][c].flagged = !field[r][c].flagged;
					if (field[r][c].flagged) gameState.buildFlag();
					else gameState.removeFlag();
				}
			}
		}
	}
}