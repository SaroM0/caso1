#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAM_BUFFER 2048

#define CIFRAR 1
#define DESCIFRAR 0



/* ---------------------------------------
   Subrutina: ajustarRangoByte a traducir. Parámetros por registro
--------------------------------------- */
__declspec(naked) int ajustarRangoByte(int valor){
    __asm{
        push ebx // guardar EBX original

        cmp ebx,255 // comparar valor con 255
        jle revisarMenor // si el valor es <= 255 revisar limite inferior
        sub ebx,256 // valor = valor - 256

    revisarMenor:
        cmp ebx,0 // comparar valor con 0
        jge finAjuste // si el valor es >= 0 terminar ajuste
        add ebx,256 // valor = valor + 256

    finAjuste:
        mov eax,ebx // retornar resultado en EAX
        pop ebx // restaurar EBX
        ret // retornar al llamador
    }
}


/* ---------------------------------------
   Rutina principal a traducir. Parámetros por pila
--------------------------------------- */
__declspec(naked) int transformarBytesClave(unsigned char *buffer,
                         int longitud,
                         unsigned char *clave,
                         int tamClave,
                         int indiceClave,
                         int modo) {
    __asm__ __volatile__ (
        "push ebp                    \n\t"   /* guardar ebp del llamador          */
        "mov  ebp, esp               \n\t"   /* nuevo marco de pila                */
        "sub  esp, 16                \n\t"   /* reservar i, valorByte, valorClave, temp */

        "mov  dword ptr [ebp-4], 0   \n\t"   /* i = 0                              */

        "for_cond:                   \n\t"
        "mov  eax, [ebp-4]           \n\t"   /* eax = i                            */
        "cmp  eax, [ebp+12]          \n\t"   /* comparar i con longitud            */
        "jge  for_end                \n\t"   /* si i >= longitud, salir del ciclo  */

        "mov  eax, [ebp-4]           \n\t"   /* eax = i                            */
        "mov  ecx, [ebp+8]           \n\t"   /* ecx = buffer                       */
        "movzx edx, byte ptr [ecx+eax] \n\t" /* edx = buffer[i] (0-255)            */
        "mov  [ebp-8], edx           \n\t"   /* valorByte = edx                    */

        "mov  eax, [ebp+24]          \n\t"   /* eax = indiceClave                  */
        "mov  ecx, [ebp+16]          \n\t"   /* ecx = clave                        */
        "movzx edx, byte ptr [ecx+eax] \n\t" /* edx = clave[indiceClave]           */
        "mov  [ebp-12], edx          \n\t"   /* valorClave = edx                   */

        "mov  eax, [ebp+28]          \n\t"   /* eax = modo                         */
        "cmp  eax, 1                 \n\t"   /* comparar con CIFRAR (1)            */
        "jne  else_descifrar         \n\t"

        "mov  eax, [ebp-8]           \n\t"   /* eax = valorByte                    */
        "add  eax, [ebp-12]          \n\t"   /* eax = valorByte + valorClave       */
        "mov  [ebp-16], eax          \n\t"   /* temp = eax                         */
        "jmp  fin_if                 \n\t"

        "else_descifrar:             \n\t"
        "mov  eax, [ebp-8]           \n\t"   /* eax = valorByte                    */
        "sub  eax, [ebp-12]          \n\t"   /* eax = valorByte - valorClave       */
        "mov  [ebp-16], eax          \n\t"   /* temp = eax                         */

        "fin_if:                     \n\t"
        "push ebx                    \n\t"   /* guardar EBX original                     */
        "mov  ebx, [ebp-16]          \n\t"   /* pasar temp por registro EBX              */
        "call ajustarRangoByte       \n\t"   /* llamar funcion con parametro por registro */
        "mov  [ebp-16], eax          \n\t"   /* temp = resultado retornado en EAX        */
        "pop  ebx                    \n\t"   /* restaurar EBX original                   */

        "mov  eax, [ebp-4]           \n\t"   /* eax = i                            */
        "mov  ecx, [ebp+8]           \n\t"   /* ecx = buffer                       */
        "mov  edx, [ebp-16]          \n\t"   /* edx = temp                         */
        "mov  byte ptr [ecx+eax], dl \n\t"   /* buffer[i] = (unsigned char) temp   */

        "mov  eax, [ebp+24]          \n\t"   /* eax = indiceClave                  */
        "add  eax, 1                 \n\t"   /* indiceClave + 1                    */
        "mov  [ebp+24], eax          \n\t"   /* indiceClave++                      */

        "mov  eax, [ebp+24]          \n\t"   /* eax = indiceClave                  */
        "cmp  eax, [ebp+20]          \n\t"   /* comparar con tamClave              */
        "jne  fin_if2                \n\t"
        "mov  dword ptr [ebp+24], 0  \n\t"   /* si son iguales, indiceClave = 0    */
        "fin_if2:                    \n\t"

        "mov  eax, [ebp-4]           \n\t"   /* eax = i                            */
        "add  eax, 1                 \n\t"   /* i + 1                              */
        "mov  [ebp-4], eax           \n\t"   /* i++                                */
        "jmp  for_cond                \n\t"   /* volver a evaluar la condición      */

        "for_end:                    \n\t"
        "mov  eax, [ebp+24]          \n\t"   /* valor de retorno = indiceClave (eax) */

        "mov  esp, ebp               \n\t"   /* liberar espacio de variables locales */
        "pop  ebp                    \n\t"   /* restaurar ebp del llamador          */
        "ret                         \n\t"   /* retornar al llamador (valor en eax) */
    );
}

/* ---------------------------------------
   Procesamiento del archivo
--------------------------------------- */
int procesarArchivo(const char *archivoEntrada,
                    const char *archivoSalida,
                    unsigned char *clave,
                    int tamClave,
                    int modo) {

    FILE *in;
    FILE *out;

    unsigned char buffer[TAM_BUFFER];

    int leidos;
    int total = 0;
    int indiceClave = 0;

    in = fopen(archivoEntrada, "rb");
    if (in == NULL) {
        printf("Error abriendo archivo de entrada\n");
        return -1;
    }

    out = fopen(archivoSalida, "wb");
    if (out == NULL) {
        printf("Error abriendo archivo de salida\n");
        fclose(in);
        return -1;
    }

    leidos = fread(buffer, 1, TAM_BUFFER, in);

    while (leidos > 0) {

        indiceClave = transformarBytesClave(
            buffer,
            leidos,
            clave,
            tamClave,
            indiceClave,
            modo
        );

        fwrite(buffer, 1, leidos, out);

        total += leidos;

        leidos = fread(buffer, 1, TAM_BUFFER, in);
    }

    fclose(in);
    fclose(out);

    return total;
}

/* ---------------------------------------
   Programa principal
--------------------------------------- */
int main(int argc, char *argv[]) {

    char *operacion;
    char *archivoEntrada;
    char *archivoSalida;
    char *claveTexto;

    unsigned char *clave;
    int tamClave;
    int modo;
    int procesados;

    if (argc != 5) {
        printf("Uso:\n");
        printf("  programa cifrar entrada salida clave\n");
        printf("  programa descifrar entrada salida clave\n");
        return 1;
    }

    operacion = argv[1];
    archivoEntrada = argv[2];
    archivoSalida = argv[3];
    claveTexto = argv[4];

    if (strcmp(operacion, "cifrar") == 0) {
        modo = CIFRAR;
    } else if (strcmp(operacion, "descifrar") == 0) {
        modo = DESCIFRAR;
    } else {
        printf("Operacion invalida\n");
        return 1;
    }

    clave = (unsigned char *) claveTexto;
    tamClave = 0;

    while (claveTexto[tamClave] != '\0') {
        tamClave++;
    }

    if (tamClave == 0) {
        printf("Clave vacia\n");
        return 1;
    }

    procesados = procesarArchivo(
        archivoEntrada,
        archivoSalida,
        clave,
        tamClave,
        modo
    );

    if (procesados >= 0) {
        printf("Archivo procesado correctamente\n");
        printf("Bytes procesados: %d\n", procesados);
    }

    return 0;
}