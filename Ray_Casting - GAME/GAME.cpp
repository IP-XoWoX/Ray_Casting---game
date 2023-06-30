#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <Windows.h>
using namespace std;


int nScreenWidth = 240;			// Размер экрана консоли X (столбцы)
int nScreenHeight = 63;			// Размер экрана консоли Y (строки)
int nMapWidth = 16;				// Размеры мира
int nMapHeight = 16;

float fPlayerX = 1.7f;			// Стартовая позиция игрока
float fPlayerY = 1.09f;
float fPlayerA = 0.0f;			// Игрок начинает ротацию
float fFOV = 3.14159f / 4.0f;	// Поле зрения
float fDepth = 16.0f;			// Максимальное расстояние рендеринга
float fSpeed = 2.0f;			// Скорость ходьбы

int main()
{
	// Создать экранный буфер
	wchar_t* screen = new wchar_t[nScreenWidth * nScreenHeight];
	HANDLE hConsole = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
	SetConsoleActiveScreenBuffer(hConsole);
	DWORD dwBytesWritten = 0;

	// Создать карту мирового пространства # = блок (стена), . = пространство
	wstring map;
	map += L"################";
	map += L"#..............#";
	map += L"#.......########";
	map += L"#..............#";
	map += L"#......##......#";
	map += L"#......##......#";
	map += L"#..............#";
	map += L"###............#";
	map += L"##.............#";
	map += L"#......####..###";
	map += L"#......#.......#";
	map += L"#......#.......#";
	map += L"#..............#";
	map += L"#......#########";
	map += L"#..............#";
	map += L"################";

	auto tp1 = chrono::system_clock::now();
	auto tp2 = chrono::system_clock::now();

	while (1)
	{
		// Нам понадобится разница во времени для каждого кадра, чтобы рассчитать
		// изменение скорости движения, чтобы обеспечить постоянное движение, поскольку трассировка лучей
		// недетерминирована.
		tp2 = chrono::system_clock::now();
		chrono::duration<float> elapsedTime = tp2 - tp1;
		tp1 = tp2;
		float fElapsedTime = elapsedTime.count();


		// Обеспечение вращения против часовой стрелки
		if (GetAsyncKeyState((unsigned short)'A') & 0x8000)
			fPlayerA -= (fSpeed * 0.75f) * fElapsedTime;

		// Обеспечение вращения по часовой стрелке
		if (GetAsyncKeyState((unsigned short)'D') & 0x8000)
			fPlayerA += (fSpeed * 0.75f) * fElapsedTime;

		// Управление движением вперед и столкновением
		if (GetAsyncKeyState((unsigned short)'W') & 0x8000)
		{
			fPlayerX += sin(fPlayerA) * fSpeed * fElapsedTime;
			fPlayerY += cos(fPlayerA) * fSpeed * fElapsedTime;
			if (map.c_str()[(int)fPlayerX * nMapWidth + (int)fPlayerY] == '#')
			{
				fPlayerX -= sin(fPlayerA) * fSpeed * fElapsedTime;
				fPlayerY -= cos(fPlayerA) * fSpeed * fElapsedTime;
			}
		}

		// Управление движением назад и столкновением
		if (GetAsyncKeyState((unsigned short)'S') & 0x8000)
		{
			fPlayerX -= sin(fPlayerA) * fSpeed * fElapsedTime;
			fPlayerY -= cos(fPlayerA) * fSpeed * fElapsedTime;
			if (map.c_str()[(int)fPlayerX * nMapWidth + (int)fPlayerY] == '#')
			{
				fPlayerX += sin(fPlayerA) * fSpeed * fElapsedTime;
				fPlayerY += cos(fPlayerA) * fSpeed * fElapsedTime;
			}
		}

		for (int x = 0; x < nScreenWidth; x++)
		{
			// Для каждого столбца вычислите угол проецируемого луча в пространство мира.
			float fRayAngle = (fPlayerA - fFOV / 2.0f) + ((float)x / (float)nScreenWidth) * fFOV;

			// Найдите расстояние до стены
			float fStepSize = 0.1f;		  // Уменьшаем размер приращения для raycasting, чтобы увеличить 										
			float fDistanceToWall = 0.0f; // разрешение

			bool bHitWall = false;		// Устанавливаем, когда луч попадает в стену
			bool bBoundary = false;		// Устанавливаем, когда луч попадает на границу между двумя блоками стены

			float fEyeX = sin(fRayAngle); // Единичный вектор для луча в пространстве игрока
			float fEyeY = cos(fRayAngle);

			// Постепенно отбрасывать луч от игрока вдоль угла луча, 
			// проверяя пересечение с блоком
			while (!bHitWall && fDistanceToWall < fDepth)
			{
				fDistanceToWall += fStepSize;
				int nTestX = (int)(fPlayerX + fEyeX * fDistanceToWall);
				int nTestY = (int)(fPlayerY + fEyeY * fDistanceToWall);

				// Проверяем, выходит ли луч за пределы
				if (nTestX < 0 || nTestX >= nMapWidth || nTestY < 0 || nTestY >= nMapHeight)
				{
					bHitWall = true;			// Просто установливаем расстояние на максимальную глубину
					fDistanceToWall = fDepth;
				}
				else
				{
					// Луч находится внутри, поэтому проверяем, является ли ячейка луча стеной
					if (map.c_str()[nTestX * nMapWidth + nTestY] == '#')
					{
						// Луч ударился о стену
						bHitWall = true;

						// Чтобы выделить границы плитки, направим луч из каждого угла плитки на игрока.
						// Чем больше этот луч совпадает с лучом рендеринга, тем ближе мы находимся
						// к границе плитки, которую мы заштрихуем, чтобы добавить деталей стенам.
						vector<pair<float, float>> p;

						// Протестируем каждый угол пораженной плитки, сохраняя расстояние
						// от игрока и вычисленное скалярное произведение двух лучей.
						for (int tx = 0; tx < 2; tx++)
							for (int ty = 0; ty < 2; ty++)
							{
								// Угол от угла к глазу
								float vy = (float)nTestY + ty - fPlayerY;
								float vx = (float)nTestX + tx - fPlayerX;
								float d = sqrt(vx * vx + vy * vy);
								float dot = (fEyeX * vx / d) + (fEyeY * vy / d);
								p.push_back(make_pair(d, dot));
							}

						// Сортируем пары от ближайших к самой дальних
						sort(p.begin(), p.end(), [](const pair<float, float>& left, const pair<float, float>& right) {return left.first < right.first; });

						// Первые два/три угла самые близкие (мы никогда не увидим все четыре)
						float fBound = 0.01;
						if (acos(p.at(0).second) < fBound) bBoundary = true;
						if (acos(p.at(1).second) < fBound) bBoundary = true;
						if (acos(p.at(2).second) < fBound) bBoundary = true;
					}
				}
			}

			// Рассчитываем расстояние до потолка и пола
			int nCeiling = (float)(nScreenHeight / 2.0) - nScreenHeight / ((float)fDistanceToWall);
			int nFloor = nScreenHeight - nCeiling;

			// Шейдерные стены на основе расстояния
			short nShade = ' ';
			if (fDistanceToWall <= fDepth / 4.0f)			nShade = 0x2588;	// Очень близко	
			else if (fDistanceToWall < fDepth / 3.0f)		nShade = 0x2593;
			else if (fDistanceToWall < fDepth / 2.0f)		nShade = 0x2592;
			else if (fDistanceToWall < fDepth)				nShade = 0x2591;
			else											nShade = ' ';		// Очень далеко

			if (bBoundary)		nShade = ' '; // Зачерняем

			for (int y = 0; y < nScreenHeight; y++)
			{
				// Каждый ряд
				if (y <= nCeiling)
					screen[y * nScreenWidth + x] = ' ';
				else if (y > nCeiling && y <= nFloor)
					screen[y * nScreenWidth + x] = nShade;
				else // Пол
				{
					// Затемнение пола в зависимости от расстояния
					float b = 1.0f - (((float)y - nScreenHeight / 2.0f) / ((float)nScreenHeight / 2.0f));
					if (b < 0.25)		nShade = '#';
					else if (b < 0.5)	nShade = 'x';
					else if (b < 0.75)	nShade = '.';
					else if (b < 0.9)	nShade = '-';
					else				nShade = ' ';
					screen[y * nScreenWidth + x] = nShade;
				}
			}
		}

		// Выводим показатели
		swprintf_s(screen, 40, L"X=%3.2f, Y=%3.2f, A=%3.2f FPS=%3.2f ", fPlayerX, fPlayerY, fPlayerA, 1.0f / fElapsedTime);

		// Вывод карты
		for (int nx = 0; nx < nMapWidth; nx++)
			for (int ny = 0; ny < nMapWidth; ny++)
			{
				screen[(ny + 1) * nScreenWidth + nx] = map[ny * nMapWidth + nx];
			}
		screen[((int)fPlayerX + 1) * nScreenWidth + (int)fPlayerY] = 'P';

		// Выводим всё
		screen[nScreenWidth * nScreenHeight - 1] = '\0';
		WriteConsoleOutputCharacter(hConsole, screen, nScreenWidth * nScreenHeight, { 0,0 }, &dwBytesWritten);
	}

	return 0;
}