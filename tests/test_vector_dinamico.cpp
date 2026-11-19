#include <cassert>
#include <cstdint>
#include <climits>
#include <cstdio>
#include "vector_dinamico.h"

int main()
{
    vectorP v = nullptr;
    crear(&v, 0);
    assert(v != nullptr && tamano(&v) == 0);
    assert(redimensionar(v, 3));
    assert(recuperar(v, 2) == 0);
    asignar(v, 1, 55);
    assert(recuperar(v, 1) == 55);
    assert(reservar(v, 32));
    assert(capacidad_vector(v) >= 32);
    assert(agregar(v, 17));
    assert(recuperar(v, 3) == 17);
    assert(redimensionar(v, 1));
    assert(redimensionar(v, 5));
    assert(recuperar(v, 1) == 0);
    assert(recuperar(v, 3) == 0);
    if (static_cast<uintmax_t>(ULONG_MAX) > SIZE_MAX / sizeof(TELEMENTO))
        assert(!reservar(v, ULONG_MAX));
    assert(tamano(&v) == 5);
    assert(redimensionar(v, 0));
    assert(agregar(v, 9));
    assert(recuperar(v, 0) == 9);
    liberar(&v);
    liberar(&v);
    assert(v == nullptr);
    std::puts("C++ vector resize: OK");
}
