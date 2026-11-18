# Зависимости этапа 1

- **GLFW 3.5.1.** Официальный архив исходников. CMake проверяет его SHA-256 и собирает статическую библиотеку. Лицензия zlib/libpng находится в `LICENSE.md` внутри архива. [GLFW 3.5.1](https://github.com/glfw/glfw/releases/tag/3.5.1)
- **GLAD 2.0.8.** Сгенерированный C-загрузчик для OpenGL 4.6 Core без расширений. Исходник и заголовки лежат в `glad/` вместе с лицензией и уведомлениями в файлах. [GLAD 2.0.8](https://github.com/Dav1dde/glad/releases/tag/v2.0.8)

Генерация GLAD из установленного `glad2==2.0.8` с фиксированной копией спецификации Khronos:

```powershell
python -m glad --api gl:core=4.6 --extensions= --reproducible --out-path ThirdParty/glad c
```

Сборка проекта использует готовый загрузчик и локальный архив GLFW. Python и подключение к сети для сборки не требуются.
