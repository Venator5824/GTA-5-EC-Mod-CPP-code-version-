#pragma once
#include "nativesheader.h"
#include <cstdint>  
#include <string>   
// Namespace schützt uns vor Namenskollisionen mit UE5 oder RAGE
namespace AbstractTypes {

    // ------------------------------------------------------------
    // 1. HANDLES (Das Herzstück)
    // ------------------------------------------------------------
    // Wir nutzen uintptr_t. 
    // Auf 64-Bit Systemen ist das groß genug für einen Pointer (UE5) 
    // UND groß genug für einen Integer (GTA V / RDR2).
    typedef std::uintptr_t GameHandle;

    // Konstante für "Kein Objekt" (in GTA 0, in UE5 nullptr)
    const GameHandle INVALID_HANDLE = 0;


    // ------------------------------------------------------------
    // 2. MATHEMATIK & POSITION
    // ------------------------------------------------------------
    // Wir definieren unseren eigenen Vektor, damit wir nicht 
    // von shv::Vector3 oder ue5::FVector abhängen.
    struct Vec3 {
        float x;
        float y;
        float z;

        // Optional: Kleiner Konstruktor für Bequemlichkeit
        Vec3(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}
    };


    // ------------------------------------------------------------
    // 3. IDENTIFIKATION (Models, Hashes)
    // ------------------------------------------------------------
    // In GTA sind das Hashes (uint32). In UE5 sind Assets oft Pfade.
    // Fürs erste bleiben wir bei 32-bit Hashes, da dein Code 
    // stark auf Hash-Vergleichen basiert (g_PersonaCache).
    typedef std::uint32_t ModelID;


    // ------------------------------------------------------------
    // 4. ZEIT & SYSTEM
    // ------------------------------------------------------------
    // Ersatz für DWORD (Windows spezifisch) -> Standard C++
    typedef std::uint32_t TimeMillis;

    // Ersatz für HWND (Windows Fenster Handle) -> Generischer Pointer
    typedef void* WindowHandle;

}

// Damit du in deiner Main nicht immer "AbstractTypes::" schreiben musst:
// (Optional, aber macht den Umstieg leichter)
using AHandle = AbstractTypes::GameHandle;
using AVec3 = AbstractTypes::Vec3;
using AModel = AbstractTypes::ModelID;