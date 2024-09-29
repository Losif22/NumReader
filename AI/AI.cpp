#include <windows.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <algorithm>

// Константа для размера изображения
const int IMAGE_SIZE = 100;

// Функция для загрузки BMP файлов
std::vector<double> LoadBMP(const wchar_t* filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::wcerr << L"Cannot open file: " << filename << std::endl;
        exit(1);
    }

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    file.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));

    int width = infoHeader.biWidth;
    int height = infoHeader.biHeight;

    file.seekg(fileHeader.bfOffBits);

    std::vector<double> pixels(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            unsigned char color[3];
            file.read(reinterpret_cast<char*>(color), 3);
            // Чёрный цвет на белом фоне
            double gray = (color[0] == 255 && color[1] == 255 && color[2] == 255) ? 0.0 : 1.0; // 0 для белого, 1 для черного
            pixels[(height - 1 - y) * width + x] = gray;
        }
    }

    return pixels;
}

// Класс нейросети
class NeuralNetwork {
private:
    std::vector<double> weights;
    int inputSize, outputSize;

public:
    NeuralNetwork(int input, int output) {
        inputSize = input;
        outputSize = output;
        weights.resize(input * output);
        srand((unsigned int)time(0));
        for (double& w : weights) {
            w = (double)rand() / RAND_MAX; // Инициализация случайными весами
        }
    }

    // Сохранение весов в файл
    void saveWeights(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Помилка при збереженні ваги у файл\n";
            return;
        }
        for (const double& weight : weights) {
            file.write(reinterpret_cast<const char*>(&weight), sizeof(weight));
        }
        file.close();
        std::cout << "Ваги збережені у файл: " << filename << "\n";
    }

    // Загрузка весов из файла
    bool loadWeights(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Не вдалося відкрити файл для завантаження ваг\n";
            return false;
        }
        for (double& weight : weights) {
            file.read(reinterpret_cast<char*>(&weight), sizeof(weight));
        }
        file.close();
        std::cout << "Ваги завантажені з файлу: " << filename << "\n";
        return true;
    }

    // Прямое распространение
    std::vector<double> feedForward(const std::vector<double>& input) {
        std::vector<double> output(outputSize, 0.0);
        for (int i = 0; i < outputSize; ++i) {
            for (int j = 0; j < inputSize; ++j) {
                output[i] += input[j] * weights[i * inputSize + j];
            }
            output[i] = 1.0 / (1.0 + exp(-output[i])); // Сигмоидальная активация
        }
        return output;
    }

    // Обратное распространение ошибки
    void backpropagation(const std::vector<double>& input, const std::vector<double>& target, double learningRate) {
        // Прямое распространение
        std::vector<double> output = feedForward(input);

        // Ошибка на выходе (target - output)
        std::vector<double> outputError(outputSize, 0.0);
        for (int i = 0; i < outputSize; ++i) {
            outputError[i] = target[i] - output[i];
        }

        // Обновление весов
        for (int i = 0; i < outputSize; ++i) {
            for (int j = 0; j < inputSize; ++j) {
                weights[i * inputSize + j] += learningRate * outputError[i] * input[j];
            }
        }
    }
};

// Глобальные переменные
NeuralNetwork* nn;
std::vector<double> userDrawing(IMAGE_SIZE* IMAGE_SIZE, 0);
bool readyToPredict = false;

// Функция для рисования в окне
void DrawInMemory(HDC hdc, HWND hwnd) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    int width = rect.right;
    int height = rect.bottom;

    for (int y = 0; y < IMAGE_SIZE; ++y) {
        for (int x = 0; x < IMAGE_SIZE; ++x) {
            int color = GetPixel(hdc, x * width / IMAGE_SIZE, y * height / IMAGE_SIZE) == RGB(0, 0, 0) ? 1.0 : 0.0;
            userDrawing[y * IMAGE_SIZE + x] = color;
        }
    }
}

// Функция обратного вызова окна
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HDC hdc;
    static HPEN pen;
    static bool isDrawing = false;
    static POINT ptPrev;

    switch (msg) {
    case WM_CREATE:
        pen = CreatePen(PS_SOLID, 10, RGB(0, 0, 0));
        break;

    case WM_LBUTTONDOWN:
        hdc = GetDC(hwnd);
        SelectObject(hdc, pen);
        isDrawing = true;
        ptPrev.x = LOWORD(lParam);
        ptPrev.y = HIWORD(lParam);
        MoveToEx(hdc, ptPrev.x, ptPrev.y, NULL); // Начальная точка линии
        break;

    case WM_LBUTTONUP:
        isDrawing = false;
        DrawInMemory(hdc, hwnd);
        ReleaseDC(hwnd, hdc);
        readyToPredict = true; // Устанавливаем флаг готовности к предсказанию
        break;

    case WM_MOUSEMOVE:
        if (isDrawing) {
            LineTo(hdc, LOWORD(lParam), HIWORD(lParam));
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_RETURN && readyToPredict) { // Если нажата клавиша "Enter" и готово к предсказанию
            // Прогнозирование числа
            std::vector<double> result = nn->feedForward(userDrawing);
            int predictedNumber = std::distance(result.begin(), std::max_element(result.begin(), result.end()));

            // Форматирование строки с результатом
            wchar_t buffer[256];
            swprintf_s(buffer, L"Розпiзнане число: %d", predictedNumber);

            // Показать сообщение
            MessageBox(hwnd, buffer, TEXT("Результат"), MB_OK);
            readyToPredict = false; // Сбрасываем флаг готовности
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Прогресс бар
void showProgressBar(int progress, int total) {
    int barWidth = 50;
    float ratio = progress / (float)total;
    int pos = barWidth * ratio;

    std::cout << "[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << int(ratio * 100.0) << " %\r";
    std::cout.flush();
}

// Главная функция
int main() {
    setlocale(LC_ALL, "uk_UA");
    nn = new NeuralNetwork(IMAGE_SIZE * IMAGE_SIZE, 10); // 10 выходов для 0-9

    // Выбор режима работы
    std::cout << "Введiть 0 для початку навчання з нуля або 1 для продовження навчання з попереднiм навчанням: ";
    int choice;
    std::cin >> choice;

    if (choice == 1) {
        // Попытка загрузить сохраненные веса
        if (!nn->loadWeights("weights.bin")) {
            std::cerr << "Помилка завантаження. Починаємо навчання з нуля.\n";
        }
        else {
            std::cout << "Ваги успiшно завантаженi.\n";
        }
    }

    if (choice == 0 || !nn->loadWeights("weights.bin")) {
        std::vector<std::vector<double>> digitImages[10];

        // Загрузка изображений для 0-9
        for (int digit = 0; digit < 10; ++digit) {
            for (int i = 1; i <= 20; ++i) {
                wchar_t filename[100];
                swprintf(filename, 100, L"./%s/%s%d.bmp",
                    digit == 0 ? L"zero" : digit == 1 ? L"one" : digit == 2 ? L"two" :
                    digit == 3 ? L"three" : digit == 4 ? L"four" : digit == 5 ? L"five" :
                    digit == 6 ? L"six" : digit == 7 ? L"seven" : digit == 8 ? L"eight" : L"nine",
                    digit == 0 ? L"zero" : digit == 1 ? L"one" : digit == 2 ? L"two" :
                    digit == 3 ? L"three" : digit == 4 ? L"four" : digit == 5 ? L"five" :
                    digit == 6 ? L"six" : digit == 7 ? L"seven" : digit == 8 ? L"eight" : L"nine",
                    i);
                digitImages[digit].push_back(LoadBMP(filename));
            }
        }

        // Обучение
        int epochs = 20000;
        double learningRate = 0.5;

        std::cout << "Навчання нейромережi...\n";
        for (int i = 0; i < epochs; ++i) {
            for (int digit = 0; digit < 10; ++digit) {
                for (const auto& image : digitImages[digit]) {
                    std::vector<double> target(10, 0.0);
                    target[digit] = 1.0; // One-hot encoding
                    nn->backpropagation(image, target, learningRate);
                }
            }
            showProgressBar(i + 1, epochs);
        }
        std::cout << "\nНавчання завершено!\n";
        nn->saveWeights("weights.bin");
    }

    // Создание и запуск оконного приложения
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = TEXT("NeuralNetWindowClass");

    RegisterClass(&wc);

    HWND hwnd = CreateWindow(wc.lpszClassName, TEXT("Малювання"), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 400, NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, SW_SHOW);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    delete nn; // Освобождение памяти
    return 0;
}
