#include <iostream>
#include <windows.h>

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		std::cout << "usage: shlex.exe <command> <command_arguments>" << std::endl;
		return 0;
	}
	else
	{
//		std::cout << argv[1] << std::endl;
//		std::cout << argv[2] << std::endl;
	}
	SHELLEXECUTEINFO sei;
	ZeroMemory( &sei, sizeof( sei ) );
	sei.lpFile = argv[1];
	sei.lpParameters = argv[2];
	sei.cbSize = sizeof( sei );
	ShellExecuteEx( &sei );

	return 0;
}