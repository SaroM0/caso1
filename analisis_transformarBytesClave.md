## 1. Análisis a fondo de `transformarBytesClave`

```c
int transformarBytesClave(unsigned char *buffer,
                         int longitud,
                         unsigned char *clave,
                         int tamClave,
                         int indiceClave,
                         int modo) {

    int i;
    int valorByte;
    int valorClave;
    int temp;

    for (i = 0; i < longitud; i++) {

        valorByte = buffer[i];
        valorClave = clave[indiceClave];

        if (modo == CIFRAR) {
            temp = valorByte + valorClave;
        } else {
            temp = valorByte - valorClave;
        }

        temp = ajustarRangoByte(temp);

        buffer[i] = (unsigned char) temp;

        indiceClave++;

        if (indiceClave == tamClave) {
            indiceClave = 0;
        }
    }

    return indiceClave;
}
```

### 1.1 Parámetros (6, todos pasados por pila)

| Parámetro     | Tipo              | Rol                                                                                                                                                                                 |
| ------------- | ----------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `buffer`      | `unsigned char *` | Puntero al bloque de bytes leído del archivo (hasta `TAM_BUFFER` bytes); se modifica **in place**: la función cifra/descifra sobre el mismo arreglo que recibió.                    |
| `longitud`    | `int`             | Cantidad de bytes válidos en `buffer` en esta llamada (lo que devolvió `fread`, no siempre `TAM_BUFFER`, ya que el último bloque del archivo puede ser más corto).                  |
| `clave`       | `unsigned char *` | Puntero a los bytes de la clave (el arreglo de caracteres pasado por línea de comandos).                                                                                            |
| `tamClave`    | `int`             | Número de bytes de la clave (longitud de la cadena, sin el `\0`).                                                                                                                   |
| `indiceClave` | `int`             | Posición dentro de `clave` en la que se debe continuar el recorrido cíclico. Es tanto un parámetro de entrada (dónde retomar) como, indirectamente, el origen del valor de retorno. |
| `modo`        | `int`             | `CIFRAR` (1) o `DESCIFRAR` (0); selecciona si la operación es suma o resta.                                                                                                         |

### 1.2 Variables locales (4)

| Variable     | Tipo  | Rol                                                                                                                                                                                                |
| ------------ | ----- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `i`          | `int` | Contador del ciclo `for`, recorre `buffer[0..longitud-1]`.                                                                                                                                         |
| `valorByte`  | `int` | Copia del byte actual del buffer, **promovido a `int`** para poder sumarlo/restarlo sin desbordar un `unsigned char` y sin problemas de signo.                                                     |
| `valorClave` | `int` | Copia del byte de la clave correspondiente a la posición cíclica actual.                                                                                                                           |
| `temp`       | `int` | Resultado intermedio de la suma/resta, antes y después de pasar por `ajustarRangoByte`. Se necesita como variable separada porque el valor puede quedar fuera del rango `[0,255]` momentáneamente. |

Nótese que todas las variables locales son `int` (4 bytes), no `unsigned char`: esto es intencional, porque `valorByte + valorClave` puede llegar hasta `255+255=510` y `valorByte - valorClave` puede bajar hasta `0-255=-255`; ninguno de esos rangos cabe en un solo byte sin truncarse antes de tiempo.

### 1.3 El ciclo

El `for` recorre cada byte del bloque leído (`i = 0 .. longitud-1`). En cada iteración:

1. Lee el byte de datos (`valorByte`) y el byte de clave en la posición actual (`valorClave`).
2. Según `modo`, combina ambos con `+` (cifrar) o `-` (descifrar).
3. Normaliza el resultado al rango `[0,255]` con `ajustarRangoByte` (ver §1.5).
4. Escribe el resultado de vuelta en `buffer[i]`, truncándolo a `unsigned char`.
5. Avanza `indiceClave` de forma **cíclica** sobre la clave.

### 1.4 Manejo de `indiceClave` (recorrido cíclico de la clave)

`indiceClave` es el corazón del cifrado de Vigenère: en lugar de usar siempre el mismo byte de clave (como César), cada byte del archivo se combina con un byte distinto de la clave, y cuando se llega al final de la clave se vuelve a empezar:

```c
indiceClave++;
if (indiceClave == tamClave) {
    indiceClave = 0;
}
```

Como `procesarArchivo` llama a `transformarBytesClave` **una vez por cada bloque** leído con `fread` (bloques de hasta 2048 bytes, TAM_BUFFER), y la clave normalmente es mucho más corta que el archivo, `indiceClave` debe "recordarse" entre llamadas: al terminar de procesar un bloque, la clave puede haber dado varias vueltas y haber quedado en cualquier posición intermedia. Por eso el valor de salida de una llamada se usa como valor de entrada de la siguiente (ver §3).

### 1.5 Modo cifrar / descifrar y el rol de `ajustarRangoByte`

`CIFRAR` y `DESCIFRAR` son constantes (`1` y `0`). El algoritmo es simétrico:

- Cifrar: `temp = valorByte + valorClave`
- Descifrar: `temp = valorByte - valorClave`

La suma/resta se hace en aritmética entera normal de `int`, así que el resultado puede salirse del rango de un byte (`[0,255]`). `ajustarRangoByte` implementa el **módulo 256** de forma manual, aprovechando que el desbordamiento nunca es mayor que un "salto" de 256 en cualquier dirección (porque ambos operandos ya están en `[0,255]`):

```c
int ajustarRangoByte(int valor) {
    if (valor > 255) valor = valor - 256;   // wrap hacia abajo
    if (valor < 0)   valor = valor + 256;   // wrap hacia arriba
    return valor;
}
```

Esto es exactamente lo que garantiza que cifrar y luego descifrar con la misma clave sea la operación inversa exacta: `(a + b) mod 256` seguido de `(resultado - b) mod 256` siempre devuelve `a`.

**Ejemplo numérico del rol de `ajustarRangoByte`** (no forma parte de la tabla de la sección 2, es solo ilustrativo):

- Cifrando: `valorByte = 200`, `valorClave = 100` → `temp = 300` → `ajustarRangoByte(300) = 300-256 = 44`.
- Descifrando ese mismo byte: `valorByte = 44`, `valorClave = 100` → `temp = -56` → `ajustarRangoByte(-56) = -56+256 = 200`. Se recupera el valor original.

---

## 2. Tabla manual — primeras 5 iteraciones

Ejemplo pequeño y verificable a mano:

- `buffer` = `"HOLA!"` → bytes ASCII `[72, 79, 76, 65, 33]`, `longitud = 5`
- `clave` = `"KEY"` → bytes ASCII `[75, 69, 89]` (K, E, Y), `tamClave = 3`
- `indiceClave` inicial = `0`
- `modo = CIFRAR`

| i   | buffer[i] inicial | indiceClave          | clave[indiceClave] | temp antes de ajustar | temp después de ajustar | buffer[i] final |
| --- | ----------------- | -------------------- | ------------------ | --------------------- | ----------------------- | --------------- |
| 0   | 72 ('H')          | 0                    | 75 ('K')           | 147                   | 147                     | 147             |
| 1   | 79 ('O')          | 1                    | 69 ('E')           | 148                   | 148                     | 148             |
| 2   | 76 ('L')          | 2                    | 89 ('Y')           | 165                   | 165                     | 165             |
| 3   | 65 ('A')          | 0 _(reset tras i=2)_ | 75 ('K')           | 140                   | 140                     | 140             |
| 4   | 33 ('!')          | 1                    | 69 ('E')           | 102                   | 102                     | 102             |

Al terminar `i=2`, `indiceClave` pasa de `2` a `3`, y como `3 == tamClave`, se reinicia a `0`; por eso en `i=3` el índice vuelve a ser `0` (clave `K`) y el ciclo de la clave empieza de nuevo. La función retorna `indiceClave = 2` (el valor tras procesar `i=4`, listo para que el **siguiente bloque** del archivo continúe exactamente donde quedó la clave).

En este ejemplo ninguna suma superó 255, por lo que `ajustarRangoByte` no tuvo que corregir nada (columna "antes" = columna "después" en todas las filas); el ejemplo del §1.5 muestra el caso en que sí actúa.

---

## 3. ¿Por qué `transformarBytesClave` retorna `indiceClave`?

`procesarArchivo` lee el archivo en bloques de hasta `TAM_BUFFER` (2048) bytes y llama a `transformarBytesClave` una vez por bloque, **encadenando el resultado**:

```c
indiceClave = transformarBytesClave(buffer, leidos, clave, tamClave, indiceClave, modo);
```

`indiceClave` representa **la posición dentro de la clave en la que se debe continuar el cifrado/descifrado** — es decir, cuántos bytes de clave se han "consumido" en total, módulo `tamClave`, hasta ese punto del archivo. Como la clave normalmente es mucho más corta que el archivo completo (y este se procesa en varios bloques independientes vía `fread`), la función necesita comunicar hacia afuera dónde quedó el recorrido cíclico de la clave para que la siguiente llamada (siguiente bloque de 2048 bytes) siga exactamente en ese punto, en lugar de reiniciar la clave en cada bloque.

Si la función no retornara ese valor (por ejemplo, si `indiceClave` se reiniciara en 0 en cada llamada), cada bloque de 2048 bytes empezaría a combinarse con la clave desde el principio, rompiendo la continuidad del flujo de clave del Vigenère entre bloques, y el descifrado ya no sería la operación inversa exacta del cifrado (se desalinearía la clave respecto a los datos a partir del segundo bloque).

En resumen: el valor retornado es el **estado** que relaciona la posición dentro del archivo con la posición dentro de la clave, y es indispensable para que el recorrido cíclico de la clave sea continuo a través de todo el archivo, sin importar en cuántos bloques se procese.

---

## 4. La pila de `transformarBytesClave`

Convención: `cdecl`, 32 bits, parámetros empujados por el llamador de derecha a izquierda (por eso `modo` se empuja primero y `buffer` al final, quedando `buffer` más cerca de la dirección de retorno). Cada parámetro y cada variable local ocupa 4 bytes (`int` o puntero de 32 bits).

### 4.1 Estado justo antes de ejecutar la primera instrucción de la función

En este punto el `CALL` del llamador ya empujó la dirección de retorno, pero la función **aún no ha ejecutado nada** (ni siquiera `push ebp`), así que `ebp` todavía pertenece al marco del llamador (`procesarArchivo`).

```
                dirección alta
        +------------------------+
        |         modo           |   <- empujado primero (parámetro más a la derecha)
        +------------------------+
        |      indiceClave       |
        +------------------------+
        |        tamClave        |
        +------------------------+
        |         clave          |
        +------------------------+
        |        longitud        |
        +------------------------+
        |         buffer         |   <- empujado último (parámetro más a la izquierda)
        +------------------------+
        |  dirección de retorno  |   <- ESP apunta aquí (la puso el CALL)
        +------------------------+
                dirección baja

        EBP  -> todavía apunta al marco de procesarArchivo (sin modificar)
        ESP  -> apunta a la dirección de retorno
```

### 4.2 Estado justo antes de ejecutar la instrucción de retorno (`ret`)

En este punto ya se ejecutó todo el cuerpo y el epílogo explícito (`mov esp, ebp` / `pop ebp`), pero **no** el `ret`. El efecto es que la pila vuelve a verse igual que en 4.1 (locales liberadas, `ebp` restaurado):

```
                dirección alta
        +------------------------+
        |         modo           |
        +------------------------+
        |      indiceClave       |
        +------------------------+
        |        tamClave        |
        +------------------------+
        |         clave          |
        +------------------------+
        |        longitud        |
        +------------------------+
        |         buffer         |
        +------------------------+
        |  dirección de retorno  |   <- ESP apunta aquí otra vez
        +------------------------+
                dirección baja

        EBP  -> restaurado al valor del llamador (por el "pop ebp")
        ESP  -> apunta a la dirección de retorno (por "mov esp, ebp; pop ebp")
        EAX  -> contiene el valor de retorno (indiceClave)
```

### 4.3 Estado de referencia durante la ejecución del cuerpo (entre 4.1 y 4.2)

Este diagrama intermedio no fue pedido explícitamente, pero es la base para justificar los desplazamientos de la sección 5:

```
                dirección alta
        +------------------------+
        |         modo           |  [ebp+28]
        +------------------------+
        |      indiceClave       |  [ebp+24]
        +------------------------+
        |        tamClave        |  [ebp+20]
        +------------------------+
        |         clave          |  [ebp+16]
        +------------------------+
        |        longitud        |  [ebp+12]
        +------------------------+
        |         buffer         |  [ebp+8]
        +------------------------+
        |  dirección de retorno  |  [ebp+4]
        +------------------------+
        |     ebp del llamador   |  [ebp+0]   <- EBP apunta aquí
        +------------------------+
        |            i            |  [ebp-4]
        +------------------------+
        |       valorByte         |  [ebp-8]
        +------------------------+
        |       valorClave        |  [ebp-12]
        +------------------------+
        |          temp           |  [ebp-16]  <- ESP apunta aquí (tope de pila)
        +------------------------+
                dirección baja
```

---

## 5. Justificación de los desplazamientos

El prólogo ejecuta:

```asm
push ebp        ; ebp del llamador queda en [ebp+0] tras el mov siguiente
mov  ebp, esp   ; ebp fijo = marco de referencia para todos los accesos
sub  esp, 16    ; reserva 4 x 4 bytes para i, valorByte, valorClave, temp
```

**Parámetros (offsets positivos respecto a `ebp`):** después de `push ebp; mov ebp, esp`, `[ebp+0]` es el `ebp` guardado y `[ebp+4]` es la dirección de retorno (dejada ahí por `call`, 4 bytes). El primer parámetro empieza justo después, en `[ebp+8]`. Como los parámetros se empujaron de derecha a izquierda, el que quedó más cerca de la dirección de retorno (offset más bajo) es el que aparece primero en la firma de la función (`buffer`), y cada parámetro siguiente está 4 bytes más arriba porque cada `int`/puntero ocupa 4 bytes en 32 bits:

- `buffer` → `[ebp+8]` (8 = 4 de `ebp` guardado + 4 de dirección de retorno)
- `longitud` → `[ebp+12]` (+4 respecto al anterior: tamaño de `buffer`, que es un puntero de 4 bytes)
- `clave` → `[ebp+16]`
- `tamClave` → `[ebp+20]`
- `indiceClave` → `[ebp+24]`
- `modo` → `[ebp+28]`

**Variables locales (offsets negativos respecto a `ebp`):** se reservan con `sub esp, 16` y se colocan en el mismo orden en que se declaran en el código C, cada una a 4 bytes de la anterior, empezando justo debajo de `ebp` (que es donde vive el `ebp` guardado, así que la primera variable local empieza en `ebp-4`, no en `ebp-0`):

- `i` → `[ebp-4]`
- `valorByte` → `[ebp-8]`
- `valorClave` → `[ebp-12]`
- `temp` → `[ebp-16]`

El tamaño total reservado (`sub esp, 16`) es exactamente `4 variables × 4 bytes`, y coincide con la distancia entre `ebp` y el tope de pila (`esp`) mientras se ejecuta el cuerpo de la función.

---

## 6. Primera traducción a ensamblador de `transformarBytesClave`

Traducción literal del algoritmo (sin optimizar), usando `__declspec(naked)` y sintaxis Intel (`-masm=intel`), tal como pide el enunciado. Todos los parámetros y variables locales se acceden **exclusivamente** mediante desplazamientos sobre `ebp` (nunca se usa un registro como "hogar" de una variable entre instrucciones; los registros solo se usan como escalones temporales para hacer la aritmética, porque x86 no permite operaciones memoria-a-memoria).

`ajustarRangoByte` todavía no fue traducida (esa parte le corresponde a la Persona 2, con paso de parámetro por registro), así que aquí se sigue llamando con la convención estándar `cdecl` (parámetro por pila), igual que en el C original.

```c
__declspec(naked) int transformarBytesClave(unsigned char *buffer,
                         int longitud,
                         unsigned char *clave,
                         int tamClave,
                         int indiceClave,
                         int modo) {
    __asm__ __volatile__ (
        "push ebp                    \n\t"   /* guardar ebp del llamador                 */
        "mov  ebp, esp               \n\t"   /* nuevo marco de pila (ebp fijo)            */
        "sub  esp, 16                \n\t"   /* reservar i, valorByte, valorClave, temp   */

        "mov  dword ptr [ebp-4], 0   \n\t"   /* i = 0                                     */

        "for_cond:                   \n\t"
        "mov  eax, [ebp-4]           \n\t"   /* eax = i                                   */
        "cmp  eax, [ebp+12]          \n\t"   /* comparar i con longitud                   */
        "jge  for_end                \n\t"   /* si i >= longitud, salir del ciclo         */

        "mov  eax, [ebp-4]           \n\t"   /* eax = i                                   */
        "mov  ecx, [ebp+8]           \n\t"   /* ecx = buffer (dirección base)             */
        "movzx edx, byte ptr [ecx+eax] \n\t" /* edx = buffer[i], byte -> entero (0..255)  */
        "mov  [ebp-8], edx           \n\t"   /* valorByte = edx                           */

        "mov  eax, [ebp+24]          \n\t"   /* eax = indiceClave                         */
        "mov  ecx, [ebp+16]          \n\t"   /* ecx = clave (dirección base)              */
        "movzx edx, byte ptr [ecx+eax] \n\t" /* edx = clave[indiceClave]                  */
        "mov  [ebp-12], edx          \n\t"   /* valorClave = edx                          */

        "mov  eax, [ebp+28]          \n\t"   /* eax = modo                                */
        "cmp  eax, 1                 \n\t"   /* comparar con CIFRAR (1)                   */
        "jne  else_descifrar         \n\t"   /* si modo != CIFRAR, ir a la rama de resta  */

        "mov  eax, [ebp-8]           \n\t"   /* eax = valorByte                           */
        "add  eax, [ebp-12]          \n\t"   /* eax = valorByte + valorClave (cifrar)     */
        "mov  [ebp-16], eax          \n\t"   /* temp = eax                                */
        "jmp  fin_if                 \n\t"   /* saltar la rama de descifrar               */

        "else_descifrar:             \n\t"
        "mov  eax, [ebp-8]           \n\t"   /* eax = valorByte                           */
        "sub  eax, [ebp-12]          \n\t"   /* eax = valorByte - valorClave (descifrar)  */
        "mov  [ebp-16], eax          \n\t"   /* temp = eax                                */

        "fin_if:                     \n\t"
        "mov  eax, [ebp-16]          \n\t"   /* eax = temp                                */
        "push eax                    \n\t"   /* parámetro de ajustarRangoByte, por pila   */
        "call ajustarRangoByte       \n\t"   /* llamada a la subrutina (cdecl)            */
        "add  esp, 4                 \n\t"   /* limpiar el parámetro empujado             */
        "mov  [ebp-16], eax          \n\t"   /* temp = valor de retorno (queda en eax)    */

        "mov  eax, [ebp-4]           \n\t"   /* eax = i                                   */
        "mov  ecx, [ebp+8]           \n\t"   /* ecx = buffer                              */
        "mov  edx, [ebp-16]          \n\t"   /* edx = temp                                */
        "mov  byte ptr [ecx+eax], dl \n\t"   /* buffer[i] = (unsigned char) temp          */

        "mov  eax, [ebp+24]          \n\t"   /* eax = indiceClave                         */
        "add  eax, 1                 \n\t"   /* eax = indiceClave + 1                     */
        "mov  [ebp+24], eax          \n\t"   /* indiceClave++                             */

        "mov  eax, [ebp+24]          \n\t"   /* eax = indiceClave                         */
        "cmp  eax, [ebp+20]          \n\t"   /* comparar con tamClave                     */
        "jne  fin_if2                \n\t"   /* si son distintos, no reiniciar            */
        "mov  dword ptr [ebp+24], 0  \n\t"   /* si son iguales, indiceClave = 0           */
        "fin_if2:                    \n\t"

        "mov  eax, [ebp-4]           \n\t"   /* eax = i                                   */
        "add  eax, 1                 \n\t"   /* eax = i + 1                               */
        "mov  [ebp-4], eax           \n\t"   /* i++                                       */
        "jmp  for_cond                \n\t"   /* volver a evaluar la condición del ciclo   */

        "for_end:                    \n\t"
        "mov  eax, [ebp+24]          \n\t"   /* valor de retorno = indiceClave (en eax)   */

        "mov  esp, ebp               \n\t"   /* liberar espacio de variables locales      */
        "pop  ebp                    \n\t"   /* restaurar ebp del llamador                */
        "ret                         \n\t"   /* retornar (valor en eax, convención cdecl) */
    );
}
```

> Nota: `ajustarRangoByte` sigue siendo la función en C tal como la entregó el enunciado (paso de `valor` por pila, retorno en `eax` generado por el compilador). Cuando la Persona 2 la traduzca a ensamblador con paso por registro, esta llamada (`push eax` / `call ajustarRangoByte` / `add esp,4`) tendrá que ajustarse para usar el registro acordado en lugar de la pila.

---

## 7. Prueba y depuración

Se hizo una primera validación individual de la traducción anterior, como punto de partida para revisarla en conjunto con la Persona 2:

- Se compiló con el comando exacto indicado en el enunciado (con la corrección del salto de línea del PDF: `-fno-unwind-tables`, no `-fnounwind-tables`):
  ```
  clang -m32 -masm=intel -fms-extensions -O0 -fno-pic -fno-unwind-tables \
        -fno-asynchronous-unwind-tables -fno-omit-frame-pointer \
        -fno-stack-protector Caso1_Programa.c -o vigenere
  ```
- Compiló sin errores (solo dos _warnings_ del enlazador por reubicaciones en `.text`, esperables al mezclar ensamblador embebido con código sin PIC).
- Se ejecutó sobre los mismos archivos Word y PDF usados en la sección 8, en modo cifrar y luego descifrar.
- Se comparó el resultado (`cmp`, byte a byte) contra la versión 100% en C: **los archivos cifrados y descifrados producidos por la versión en ensamblador son binariamente idénticos a los de la versión en C**, y los SHA-256 coinciden exactamente con los de la tabla de la sección 8.

Esto da una base ya verificada, pero **falta la sesión conjunta con la Persona 2**: revisar el código instrucción por instrucción explicando cada línea en voz alta, probar casos borde a propósito (`longitud = 0`, `tamClave = 1`, un bloque que cruce el límite de `TAM_BUFFER`), y dejar la traducción de `ajustarRangoByte` (por registro) integrada para volver a correr esta misma comparación end-to-end.

---

## 8. Pruebas con archivos reales

Programa compilado con `gcc -O0 -o vigenere Caso1_Programa.c` (versión en C sin modificar, para las pruebas funcionales de la sección 3 del enunciado). Clave usada: `Caso1Clave`.

### 8.1 Word (`prueba_word.docx`, 1051 bytes)

| Campo                          | Valor                                                                                                                                                                                                                                                        |
| ------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Comando de cifrado             | `./vigenere cifrar prueba_word.docx prueba_word_cifrado.docx Caso1Clave`                                                                                                                                                                                     |
| Comando de descifrado          | `./vigenere descifrar prueba_word_cifrado.docx prueba_word_descifrado.docx Caso1Clave`                                                                                                                                                                       |
| Tamaño original                | 1051 bytes                                                                                                                                                                                                                                                   |
| Tamaño cifrado                 | 1051 bytes                                                                                                                                                                                                                                                   |
| Tamaño descifrado              | 1051 bytes                                                                                                                                                                                                                                                   |
| ¿Abre el cifrado?              | **No.** `file` lo reporta como `data` (deja de reconocerse como "Microsoft Word 2007+"); `unzip -t` no encuentra el directorio central del ZIP ("cannot find zipfile directory") — un `.docx` es un ZIP, y el cifrado ya no tiene una estructura ZIP válida. |
| ¿Abre el descifrado?           | **Sí.** `file` vuelve a reportarlo como `Microsoft Word 2007+`; `unzip -t` lo valida sin errores.                                                                                                                                                            |
| SHA-256 original               | `1f71bddb03c32341a4da227b813f3b50d6ffba6b9db80c3e48c96bbeddd5ed29`                                                                                                                                                                                           |
| SHA-256 cifrado                | `8e746682f3f2d40cd7b9c023250202e5a3cc207df4edb0594bbd5fbcff1a5fcf`                                                                                                                                                                                           |
| SHA-256 descifrado             | `1f71bddb03c32341a4da227b813f3b50d6ffba6b9db80c3e48c96bbeddd5ed29` (idéntico al original)                                                                                                                                                                    |
| Primeros 16 bytes — original   | `50 4b 03 04 14 00 00 00 08 00 e7 b0 1a 5d 17 98`                                                                                                                                                                                                            |
| Primeros 16 bytes — cifrado    | `93 ac 76 73 45 43 6c 61 7e 65 2a 11 8d cc 48 db`                                                                                                                                                                                                            |
| Primeros 16 bytes — descifrado | `50 4b 03 04 14 00 00 00 08 00 e7 b0 1a 5d 17 98` (idéntico al original)                                                                                                                                                                                     |

### 8.2 PDF (`Caso1_Enunciado.pdf` usado como `prueba_pdf.pdf`, 162583 bytes)

| Campo                          | Valor                                                                                                                                                                                                                                  |
| ------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Comando de cifrado             | `./vigenere cifrar prueba_pdf.pdf prueba_pdf_cifrado.pdf Caso1Clave`                                                                                                                                                                   |
| Comando de descifrado          | `./vigenere descifrar prueba_pdf_cifrado.pdf prueba_pdf_descifrado.pdf Caso1Clave`                                                                                                                                                     |
| Tamaño original                | 162583 bytes                                                                                                                                                                                                                           |
| Tamaño cifrado                 | 162583 bytes                                                                                                                                                                                                                           |
| Tamaño descifrado              | 162583 bytes                                                                                                                                                                                                                           |
| ¿Abre el cifrado?              | **No.** `file` lo reporta como `data` (deja de reconocerse como "PDF document"); `pdfinfo` falla con `Syntax Warning: May not be a PDF file`, `Illegal character ')'`, `Couldn't find trailer dictionary`, `Couldn't read xref table`. |
| ¿Abre el descifrado?           | **Sí.** `file` vuelve a reportarlo como `PDF document, version 1.3, 7 page(s)`; `pdfinfo` lee metadatos normalmente.                                                                                                                   |
| SHA-256 original               | `e501c78a0babb0121194e062ed132b75c3590c45cd50a8d92c305384af46d81d`                                                                                                                                                                     |
| SHA-256 cifrado                | `396cc30a5222ccda0346c4e578aa10a75bd3e39f327da4c7bf6cec72352cb23d`                                                                                                                                                                     |
| SHA-256 descifrado             | igual al original (idéntico al de la fila "original")                                                                                                                                                                                  |
| Primeros 16 bytes — original   | `25 50 44 46 2d 31 2e 33 0a 25 c4 e5 f2 e5 eb a7`                                                                                                                                                                                      |
| Primeros 16 bytes — cifrado    | `68 b1 b7 b5 5e 74 9a 94 80 8a 07 46 65 54 1c ea`                                                                                                                                                                                      |
| Primeros 16 bytes — descifrado | `25 50 44 46 2d 31 2e 33 0a 25 c4 e5 f2 e5 eb a7` (idéntico al original)                                                                                                                                                               |

**Sobre SHA-256:** es una función _hash_ criptográfica que produce una huella de 256 bits (64 caracteres hex) a partir de cualquier archivo; cualquier cambio de un solo bit en el archivo produce, con altísima probabilidad, un hash completamente distinto ("efecto avalancha"), y es prácticamente imposible que dos archivos distintos produzcan el mismo hash. Aquí sirve para **verificar objetivamente**, sin depender de "abrir el archivo a ojo", que el archivo descifrado es _exactamente_ igual, bit a bit, al original: si los SHA-256 del original y del descifrado coinciden, hay evidencia criptográficamente fuerte de que el proceso cifrar→descifrar no alteró ni un solo bit del contenido.

---

## 9. Borrador — respuestas a, b, c del informe

### a. ¿El tamaño del archivo cambia al cifrarlo?

No. En las dos pruebas (Word: 1051 → 1051 bytes; PDF: 162583 → 162583 bytes) el tamaño del archivo cifrado es exactamente igual al del original. Esto es consecuencia directa del algoritmo: `transformarBytesClave` recorre el buffer byte por byte y, por cada byte leído (`buffer[i]`), escribe exactamente un byte de salida (`buffer[i] = (unsigned char) temp`), transformado mediante suma o resta módulo 256 (`ajustarRangoByte`) con un byte de la clave. No se insertan bytes de relleno, no se eliminan bytes, y no se agrupan o dividen bytes de entrada para producir la salida: la transformación es una función byte-a-byte, biyectiva dentro del rango `[0,255]` para una clave fija. Como además `procesarArchivo` escribe (`fwrite`) exactamente los mismos `leidos` bytes que leyó (`fread`) en cada bloque, la cantidad total de bytes del archivo de salida es, bloque a bloque, igual a la de entrada, y por tanto el archivo completo conserva su tamaño original.

### b. Un archivo cifrado puede conservar el mismo nombre y extensión que el original, y aun así la aplicación puede no reconocerlo. ¿Por qué?

Porque la extensión del archivo (`.docx`, `.pdf`) es únicamente una convención externa —una etiqueta en el nombre— que el sistema operativo usa para sugerir con qué aplicación abrirlo; **no forma parte de la representación binaria del contenido** ni es verificada por la aplicación antes de intentar interpretar el archivo. La aplicación (o el propio parser del formato) valida el contenido mirando estructuras internas específicas: firmas/cabeceras al inicio del archivo (por ejemplo, un PDF debe comenzar con la secuencia de bytes `%PDF-1.x`), tablas de referencias internas (la tabla `xref` y el diccionario `trailer` en PDF), o, en el caso de `.docx` (que en realidad es un archivo ZIP con XML adentro), la firma de ZIP `PK\x03\x04` y un directorio central de ZIP válido más los archivos XML de Office Open XML.

Al cifrar con Vigenère, **todos** los bytes del archivo se transforman, incluidos los de esas cabeceras y estructuras internas (el algoritmo no distingue entre "bytes de datos" y "bytes de estructura": todo es un arreglo de bytes). El resultado es que, aunque el archivo se renombre conservando `.docx` o `.pdf`, su contenido ya no cumple la especificación del formato: la evidencia recogida lo confirma — `file` deja de identificar ambos archivos cifrados (los reporta como `data` genérico), `pdfinfo` falla al no encontrar un `trailer`/`xref` válidos, y `unzip -t` no encuentra un directorio central de ZIP en el `.docx` cifrado. La aplicación intenta parsear el archivo según las reglas de su formato, no según su nombre, y esas reglas ya no se cumplen.

### c. Compare los primeros 16 bytes del archivo original, cifrado y descifrado. ¿Qué evidencia proporcionan?

En ambos archivos de prueba, los primeros 16 bytes cambian **por completo** entre el original y el cifrado (por ejemplo, en el PDF: `25 50 44 46 2d 31 2e 33 0a 25 c4 e5 f2 e5 eb a7` pasa a `68 b1 b7 b5 5e 74 9a 94 80 8a 07 46 65 54 1c ea`; en el Word: `50 4b 03 04 14 00 00 00 08 00 e7 b0 1a 5d 17 98` pasa a `93 ac 76 73 45 43 6c 61 7e 65 2a 11 8d cc 48 db`). Esto evidencia que el algoritmo transforma **todos** los bytes del archivo, sin excepción y sin reconocer ninguna estructura especial al inicio del archivo (como la firma `%PDF` o `PK`); simplemente aplica `(byte + clave[i mod tamClave]) mod 256` a cada posición, independientemente de si esos bytes forman parte de una cabecera crítica para el formato o de datos "de contenido".

Por otro lado, los primeros 16 bytes del archivo **descifrado son idénticos, byte a byte**, a los del original (confirmado además por la igualdad de SHA-256 en ambos casos), lo que evidencia que el proceso de descifrado (`temp = valorByte - valorClave`, seguido del mismo `ajustarRangoByte`) es exactamente la operación inversa de la de cifrado: al restar el mismo byte de clave que se sumó, y con la misma corrección de módulo 256, se recupera con exactitud el valor original de cada byte. En conjunto, la comparación demuestra dos cosas: (1) que el cifrado es una transformación reversible byte a byte dependiente únicamente de la clave y la posición (no del "significado" del byte dentro del formato), y (2) que la implementación del descifrado en `transformarBytesClave`/`ajustarRangoByte` recupera el archivo original sin ninguna pérdida de información.

---

## Archivos de esta prueba

Para referencia, los binarios y archivos usados en la sección 8 quedaron en:
`/tmp/claude-1000/-home-saro-university-tic-caso1/affd72b1-f5fc-49e0-a488-0ad1d2d279ba/scratchpad/pruebas/`
(carpeta temporal de la sesión; no forma parte del repositorio). El código con la traducción a ensamblador ya compilada y probada está en
[`Caso1_Programa_transformarBytesClave_draft.c`](Caso1_Programa_transformarBytesClave_draft.c) de este mismo directorio, como punto de partida para la sesión conjunta con la Persona 2 (sección 7).
