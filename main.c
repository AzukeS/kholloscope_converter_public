#include "converter.h"

int main(int argc, char** argv)
{
    if (argc != 3 && argc != 2)
    {
        fprintf(stderr, "Usage: %s <kholloscope_path.csv> <group_number(digit +letter)>, "
                        "if no group_number specified, it will execute from group_number "
                        "= 1 to group_number = 16\n", argv[0]);
        return 1;
    }

    const char* kholloscope_path = argv[1];

    if (argc == 2)
    {
        char *letters = "abc";
        int nb_groups = 16;
        for (int i = 1; i <= nb_groups; i++)
        {
            for (int j = 0; j <= 2; j++)
            {
                export_group(kholloscope_path, i, letters[j]);
            }
        }
    }
    else
    {
        const char* group_number = argv[2];
	if (strlen(argv[2])== 2)
		export_group(kholloscope_path, group_number[0]-'0', group_number[1]);
	else
	{
		int number;

		number = group_number[1] - '0' + 10;
		export_group(kholloscope_path, number, group_number[3]);
	}
    }
    return 0;
}
