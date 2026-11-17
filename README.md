# ChromaStack

4 modüllü (Character, Movement, Diffusion, Texture), her modülde 5 stereo efekt, sıralaması değiştirilebilir multi-efekt (JUCE 8, VST3 / AU / Standalone).

## Derleme (CMake 3.22+, internet gerekli: JUCE otomatik indirilir)

macOS (Xcode Command Line Tools):
    cmake -B build -G Xcode
    cmake --build build --config Release

Windows (Visual Studio 2022, "Desktop development with C++"):
    cmake -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release

Çıktılar: build/ChromaStack_artefacts/Release/VST3/ChromaStack.vst3
- macOS: ~/Library/Audio/Plug-Ins/VST3/ klasörüne kopyala
- Windows: C:\Program Files\Common Files\VST3\ klasörüne kopyala (yönetici izni gerekir)

Not: Dsp.h'deki efektler JUCE'suz sanity testten geçti (NaN/taşma yok); tam eklenti derlemesi ve DAW testi sizin makinenizde yapılmalı.
