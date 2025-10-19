//
// Created by azuke on 18/10/2025.
//

#ifndef KHOLLOSCOPE_CONVERTER_CONVERTER_H
#define KHOLLOSCOPE_CONVERTER_CONVERTER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#define MAX_LINE 10000
#define MAX_CELLS 256
#define MAX_DATE 64
#define MAX_WEEKS 100

void export_group(const char *input_file, int group_digit, char group_letter);

#endif //KHOLLOSCOPE_CONVERTER_CONVERTER_H
