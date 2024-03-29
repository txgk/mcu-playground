Эта программа предназначена для работы в POSIX окружении, поэтому для того,
чтобы успешно ей пользоваться, необходима POSIX-совместимая операционная
система. Подавляющая часть Linux дистрибутивов и BSD систем должна подойти.

Единственной зависимостью этой программы является библиотека ncurses. Помимо
самой библиотеки должны быть установлены её заголовочные файлы и сопутствующие
ей утилиты (обязательно должна быть установлена программа stty).

Для сборки программы необходимо в папке этого проекта выполнить команду make.
Для пересборки программы необходимо сначала выполнить команду make clean, а
затем make. Результатом удачной сборки является появившийся в папке этого
проекта исполняемый файл main без выведенных в консоль сообщений о каких-либо
ошибках при компиляции.

Программа анализирует данные I2C обмена, записанные/записывающиеся в файл. По
умолчанию путь для файла с этими данными - "/tmp/i2c-dumps/recent", но при
помощи параметра командной строки "-l" можно указать произвольный путь к файлу.

Для получения данных I2C обмена используется скрипт "start-listening.sh".
При его запуске начинается запись получаемых по serial порту данных в указанный
в начальном сообщении файл (также к этому файлу создаётся символическая ссылка
"/tmp/i2c-dumps/recent", что позволяет анализировать недавно полученный лог без
указания пути на него). В процессе записи данных I2C обмена в файл параллельно
можно анализировать этот же файл при помощи анализатора (программа main).

Управление интерфейсом анализатора осуществляется при помощи следующих клавиш:
вверх - выбрать предыдущую запись,
вниз - выбрать следующую запись,
влево - вернуться в предыдущее меню,
вправо - открыть выбранную запись,
ввод - открыть выбранную запись,
запятая - убрать последний пакет из истории,
точка - добавить следующий пакет в историю,
< - оставить в истории только первый пакет,
> - вернуть в историю все имеющиеся пакеты,
1 - установить скорость листания истории 1,
2 - установить скорость листания истории 5,
3 - установить скорость листания истории 10,
4 - установить скорость листания истории 50,
5 - установить скорость листания истории 100,
6 - установить скорость листания истории 500,
7 - установить скорость листания истории 1000,
8 - установить скорость листания истории 5000,
9 - установить скорость листания истории 10000,
0 - установить скорость листания истории 50000,
q - выйти из программы.
