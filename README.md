Инструкция по сборке и тестированию:

Установите библиотеку liburing, если она еще не установлена, и cmake:

sudo apt-get install liburing-dev cmake

Скачайте и соберите проект 

mkdir build
cd build
cmake ..
make

Запустите сервер, указав порт в аргументе командной строки:

./server <port>

Подключитесь и отправьте сообщение (для выхода используйте ctrl+] затем q)

telnet localhost <port>

Отпишитесь мне в телеграмм о ваших успехах в освоении компьютерсайнса @mikhail_golovach
Хорошего дня.
