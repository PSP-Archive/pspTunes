=====

----------------------------------------------------------
---------	pspTunes - MP3 Player		----------
----------------------------------------------------------

pspTunes es una aplicación homebrew para el PlayStation Portable, que permite mediante el uso de jTunes, poseer una biblioteca de música, con la cual se puede filtrar la música por distintas categorías basadas en los tags ID3 del formato MP3.

En dado caso no se poseea o no se desee utilizar jTunes, se mantiene la compatibilidad con la música que se posea. Se debe ir a la categoría "Carpeta".


=============================
1. Instalación
=============================

Para instalar la aplicacion, conecte la consola mediante el modo USB.
Luego copie la carpeta "PSP" que esta dentro de la carpeta "bin" de este directorio en la raiz de la MemoryStick.
Si se pregunta que si se desea sobreescribir, se elije "Si".
Nota: la sobreescritura no afecta en realidad ningún archivo y/o carpeta que se tuviese anteriormente.


=============================
2. Ejecución
=============================

Para ejecutar la aplicación, vaya a la categoría "Juego" del XMB y luego elija "MemoryStick".
Luego elija la aplicación llamada "pspTunes - MP3 Player"

Nota: Para que esta aplicacion se pueda ejecutar, el PSP necesita alguna manera de ejecutar aplicaciones homebrew. El usuario es responsable de la manera en que esto sea posible.


=============================
3. Uso
=============================

Si no está seguro de como utilizar la aplicación, presione el boton "SELECT" para mostrar un panel de ayuda donde estará la función de cada botón.

Se necesita el uso de jTunes para poder utilizar la mayoria de funcionalidades de filtración. 

=============================
4. Agradecimientos
=============================

Se agradece a:
- "Sakya" por su modificación de la Old School library.
- "mowglisanu" por su libaudio
- Los creadores de las librerias TinyXML y TinyXML++
- Los creadores de la SDK para PSP
- A la scene de homebrew para PSP



=============================
5. Sobre los creadores y la aplicación
=============================

Los creadores de este programa son el equipo Gamma Ray.
La aplicación nace como una propuesta de proyecto para una materia de la universidad.
Toda la información necesaria esta en el sitio:
http://sites.google.com/site/gammaraypei

Se esperá seguir con el desarrollo de esta aplicación para hacerse más independiente y más funcional.

NOTA: esta aplicación no está ligada a la aplicación flash "pspTunes" hecha por Bigbondfan.
Para esa aplicación ir a: http://www.qj.net/qjnet/psp/psptunes-v20-released.html

Al momento de hacerse esta aplicación no se sabia de esta otra aplicacion.

**********************************


=============================
6. Para desarrolladores
=============================

- Se permite el uso de este código de acuerdo a lo mencionado en COPYING.txt
- Se debe poseer la PSPSDK ya configurada.
- Dentro de la carpeta "lib" se encuentran las librerias usadas, cada una con las instrucciones para compilarse y utilzarse.
- De libaudio solamente se utilizó la parte para reproducir MP3 y se utilizaron los archivos .h y .c directamente, no se compilo la libreria.
- Por cuestiones de tiempo el código no está muy comentado y ciertas partes se hicieron con código desordenado, se espera mejorar con futuras versiones.