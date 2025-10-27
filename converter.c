#include "converter.h"
#include <time.h>

typedef struct {
    char week_num[MAX_DATE];
    char date[MAX_DATE];
} WeekDate;

// Variables globales pour le mapping des semaines
int global_week_map[MAX_CELLS];
int global_week_count = 0;

// Enlève le BOM UTF-8 si présent
void remove_bom(char *str) {
    if ((unsigned char)str[0] == 0xEF &&
        (unsigned char)str[1] == 0xBB &&
        (unsigned char)str[2] == 0xBF) {
        memmove(str, str + 3, strlen(str + 3) + 1);
    }
}

void clean_cell(char *cell)
{
    char *dst = cell;
    for (char *src = cell; *src; src++) {
        if (*src != '"' && *src != '\r')
            *dst++ = *src;
    }
    *dst = 0;

    // Enlève les espaces en début et fin
    char *start = cell;
    while (*start == ' ' || *start == '\t') start++;
    if (start != cell) memmove(cell, start, strlen(start) + 1);

    int len = strlen(cell);
    while (len > 0 && (cell[len-1] == ' ' || cell[len-1] == '\t')) {
        cell[len-1] = 0;
        len--;
    }
}

// Lit une ligne complète du CSV en gérant les guillemets multi-lignes
int read_full_csv_line(FILE *f, char *buffer, int max_size) {
    buffer[0] = 0;
    int pos = 0;
    int in_quotes = 0;

    while (pos < max_size - 1) {
        int c = fgetc(f);
        if (c == EOF) {
            if (pos == 0) return 0;
            break;
        }

        if (c == '"') {
            in_quotes = !in_quotes;
        }

        if (c == '\n' && !in_quotes) {
            buffer[pos] = 0;
            return 1;
        }

        buffer[pos++] = c;
    }
    buffer[pos] = 0;
    return 1;
}

int parse_csv_line(const char *line, char **cells)
{
    int n = 0;
    int in_quotes = 0;
    char buffer[MAX_LINE];
    int buf_index = 0;

    for (const char *p = line;; p++) {
        char c = *p;
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if ((c == ',' && !in_quotes) || c == '\0') {
            buffer[buf_index] = 0;
            cells[n] = malloc(buf_index+1);
            strcpy(cells[n], buffer);
            clean_cell(cells[n]);
            n++;
            buf_index = 0;
        } else {
            if (buf_index < MAX_LINE - 1) {
                buffer[buf_index++] = c;
            }
        }
        if (c == '\0') break;
    }
    return n;
}

void free_cells(char **cells, int n)
{
    for (int i = 0; i < n; i++) free(cells[i]);
}

int read_weeks(FILE *f, WeekDate *weeks) {
    char line[MAX_LINE * 10];
    int count = 0;
    rewind(f);

    for (int i = 0; i < MAX_CELLS; i++) {
        global_week_map[i] = -1;
    }

    while (read_full_csv_line(f, line, sizeof(line))) {
        remove_bom(line);

        if (strncmp(line,"Semaines",8)==0) {
            char *cells[MAX_CELLS];
            int n = parse_csv_line(line, cells);

            printf("=== Parsing ligne Semaines: %d colonnes ===\n", n);

            // Crée le mapping et lit les semaines
            for (int i = 5; i < n; i++) {
                if (strlen(cells[i]) > 0) {
                    char *newline = strchr(cells[i], '\n');
                    if (newline) {
                        int week_len = newline - cells[i];
                        strncpy(weeks[count].week_num, cells[i], week_len);
                        weeks[count].week_num[week_len] = 0;
                        strcpy(weeks[count].date, newline + 1);

                        printf("Semaine %d: num='%s' date='%s'\n", count, weeks[count].week_num, weeks[count].date);

                        global_week_map[i] = count;  // MAP colonne -> index
                        count++;
                    }
                } else {
                    global_week_map[i] = -1;  // Colonne vide
                }
            }

            global_week_count = count;
            free_cells(cells, n);
            break;
        }
    }
    return count;
}

int is_future_date_with_offset(const char *date_str, int day_offset)
{
    if (strlen(date_str) < 8) return 0;

    // Parse la date
    int day, month, year;
    sscanf(date_str, "%d/%d/%d", &day, &month, &year);
    year += 2000;

    // Crée une structure tm pour la date de la khôlle
    struct tm kholle_date = {0};
    kholle_date.tm_mday = day;
    kholle_date.tm_mon = month - 1;
    kholle_date.tm_year = year - 1900;
    kholle_date.tm_hour = 12;

    kholle_date.tm_mday += day_offset;

    time_t kholle_time = mktime(&kholle_date);

    time_t now = time(NULL);
    struct tm *current_time = localtime(&now);
    current_time->tm_hour = 0;
    current_time->tm_min = 0;
    current_time->tm_sec = 0;
    time_t current_day = mktime(current_time);

    return kholle_time >= current_day;
}

// Convertit un jour (Lu, Ma, Me, Je, Ve) en nombre de jours depuis le lundi
int day_to_offset(const char *jour) {
    if (strcmp(jour, "Lu") == 0) return 0;
    if (strcmp(jour, "Ma") == 0) return 1;
    if (strcmp(jour, "Me") == 0) return 2;
    if (strcmp(jour, "Je") == 0) return 3;
    if (strcmp(jour, "Ve") == 0) return 4;
    return 0;
}

// Détermine si la date est en heure d'été (dernier dimanche de mars au dernier dimanche d'octobre)
int is_dst(int day, int month, int year) {
    if (month < 3 || month > 10) return 0;
    if (month > 3 && month < 10) return 1;

    struct tm temp = {0};
    temp.tm_year = year - 1900;
    temp.tm_mon = month - 1;
    temp.tm_mday = day;
    mktime(&temp);

    int wday = temp.tm_wday;
    int last_sunday = day + (7 - wday) % 7;

    while (last_sunday + 7 <= 31) {
        temp.tm_mday = last_sunday + 7;
        if (mktime(&temp) == -1) break;
        if (temp.tm_mon != month - 1) break;
        last_sunday += 7;
    }

    if (month == 3) return day >= last_sunday;
    if (month == 10) return day < last_sunday;

    return 0;
}

void add_days_to_date_with_time(char *output, const char *week_date, const char *day, const char *horaire) {
    int day_num, month, year;
    sscanf(week_date, "%d/%d/%d", &day_num, &month, &year);
    year += 2000;

    struct tm date = {0};
    date.tm_year = year - 1900;
    date.tm_mon = month - 1;
    date.tm_mday = day_num + day_to_offset(day);

    // Lecture flexible des horaires : "13h", "13h30", "8h5", etc.
    int heure = 0, minute = 0;
    sscanf(horaire, "%dh%d", &heure, &minute);

    date.tm_hour = heure;
    date.tm_min = minute;
    date.tm_sec = 0;
    date.tm_isdst = -1;

    mktime(&date);

    char temp[64];
    strftime(temp, sizeof(temp), "%Y-%m-%d %H:%M", &date);

    int offset = is_dst(date.tm_mday, date.tm_mon + 1, date.tm_year + 1900) ? 2 : 1;
    sprintf(output, "%s+0%d:00", temp, offset);
}

// Convertit une date DD/MM/YY en format Todoist (YYYY-MM-DD)
void format_date_for_todoist(const char *input_date, char *output_date) {
    if (strlen(input_date) < 8) {
        strcpy(output_date, "");
        return;
    }

    char day[3], month[3], year[3];
    strncpy(day, input_date, 2); day[2] = 0;
    strncpy(month, input_date + 3, 2); month[2] = 0;
    strncpy(year, input_date + 6, 2); year[2] = 0;

    sprintf(output_date, "20%s-%s-%s", year, month, day);
}

int match_groupe(const char *cell, int group_digit, char group_letter, const char *discipline)
{
    if (!cell || strlen(cell) == 0)
        return 0;

    int cell_digit = 0;
    int cell_letter = 0;

    if (isdigit(cell[0])) {
        sscanf(cell, "%d", &cell_digit);

        for (int i = 0; cell[i]; i++)
            if (isalpha(cell[i])) {
                cell_letter = cell[i];
                break;
            }
    }
    if (cell_digit != group_digit) return 0;

    // Pour le Français, il faut matcher chiffre + lettre
    if (strcmp(discipline, "Français") == 0 || strcmp(discipline, "Francais") == 0) {
        return cell_letter == group_letter;
 }

    // Pour les autres matières, seul le chiffre compte
    return 1;
}

// Nettoie et formate le nom du professeur
void format_prof_name(const char *raw_prof, char *formatted_prof) {
    char temp[256];
    strcpy(temp, raw_prof);

    // Remplace les retours à la ligne par des espaces
    for (char *p = temp; *p; p++) {
        if (*p == '\n') *p = ' ';
    }

    // Enlève les espaces multiples
    char *dst = formatted_prof;
    int last_was_space = 0;
    for (char *src = temp; *src; src++) {
        if (*src == ' ') {
            if (!last_was_space) {
                *dst++ = ' ';
                last_was_space = 1;
            }
        } else {
            *dst++ = *src;
            last_was_space = 0;
        }
    }
    *dst = 0;

    // Enlève les espaces en début et fin
    char *start = formatted_prof;
    while (*start == ' ') start++;
    if (start != formatted_prof) memmove(formatted_prof, start, strlen(start) + 1);

    int len = strlen(formatted_prof);
    while (len > 0 && formatted_prof[len-1] == ' ') {
        formatted_prof[len-1] = 0;
        len--;
    }
}

void export_group(const char *input_file, int group_digit, char group_letter)
{
    FILE *f = fopen(input_file,"r");
    if (!f)
    {
        perror("Erreur ouverture fichier");
        return;
    }

    char output_name[128];
    sprintf(output_name,"Output_csvs/todoist_%d%c.csv", group_digit, group_letter);
    FILE *out = fopen(output_name,"w");
    if (!out)
    {
        perror("Erreur création fichier de sortie");
        fclose(f);
        return;
    }

    fprintf(out,"TYPE,CONTENT,PRIORITY,INDENT,AUTHOR,RESPONSIBLE,DATE,DATE_LANG,TIMEZONE\n");

    WeekDate weeks[MAX_WEEKS];
    int n_weeks = read_weeks(f, weeks);

    printf("=== Nombre de semaines lues: %d ===\n", n_weeks);

    rewind(f);
    char line[MAX_LINE * 10];
    int line_num = 0;
    int entries_found = 0;

    // Skips
    read_full_csv_line(f, line, sizeof(line));
    line_num++;

    read_full_csv_line(f, line, sizeof(line));
    line_num++;

    read_full_csv_line(f, line, sizeof(line));
    line_num++;

    // Variable pour stocker la dernière discipline non vide
    char last_discipline[256] = "";

    while (read_full_csv_line(f, line, sizeof(line)))
    {
        line_num++;
        remove_bom(line);

        char *cells[MAX_CELLS];
        int n = parse_csv_line(line, cells);

        if (n < 6)
        {
            free_cells(cells, n);
            continue;
        }

        char *discipline = cells[0];
        char *prof = cells[1];
        char *jour = cells[2];
        char *horaire = cells[3];
        char *salle = cells[4];

        // Si discipline est vide, utilise la dernière discipline
        if (strlen(discipline) > 0)
        {
            strcpy(last_discipline, discipline);
        }
        else
        {
            discipline = last_discipline;
        }

        // Ligne vide complète
        if (strlen(discipline) == 0 || strlen(jour) == 0)
        {
            free_cells(cells, n);
            continue;
        }

        for (int i = 5; i < n; i++)
        {
            // Skip les colonnes vides
            if (strlen(cells[i]) == 0)
            {
                continue;
            }

            // Vérifie que la colonne a un mapping valide
            if (global_week_map[i] == -1 || global_week_map[i] >= n_weeks)
            {
                continue;
            }

            int week_idx = global_week_map[i];

            if (match_groupe(cells[i], group_digit, group_letter, discipline))
            {
                int day_offset = day_to_offset(jour);

                if (is_future_date_with_offset(weeks[week_idx].date, day_offset))
                {
                    // Formatte le nom du prof
                    char prof_clean[256];
                    if (strlen(prof) > 0)
                    {
                        format_prof_name(prof, prof_clean);
                    }
                    else
                    {
                        strcpy(prof_clean, "");
                    }

                    char date_todoist[64];
                    add_days_to_date_with_time(date_todoist, weeks[week_idx].date, jour, horaire);

                    char task_content[1024];
                    if (strlen(prof_clean) > 0)
                    {
                        if (strcmp(discipline, "Anglais") == 0)
                        {
                            snprintf(task_content, sizeof(task_content), "Khôlle d'%s en %s avec %s",
                                    discipline, salle, prof_clean);
                        }
                        else
                        {
                            snprintf(task_content, sizeof(task_content), "Khôlle de %s en %s avec %s",
                                    discipline, salle, prof_clean);
                        }
                    }
                    else
                    {
                        if (strcmp(discipline, "Français") == 0)
                            snprintf(task_content, sizeof(task_content), "Khôlle de Français en %s avec M. LASSUS-MINVIELLE", salle);

                        else
                            snprintf(task_content, sizeof(task_content), "Khôlle de %s en %s",discipline, salle);
                    }

                    // Format Todoist: TYPE,CONTENT,PRIORITY,INDENT,AUTHOR,RESPONSIBLE,DATE,DATE_LANG,TIMEZONE
                    fprintf(out, "task,\"%s\",1,1,,,\"%s\",fr,\n", task_content, date_todoist);

                    entries_found++;
                }
            }
        }

        free_cells(cells, n);
    }

    fclose(f);
    fclose(out);

    printf("\n=== RÉSUMÉ ===\n");
    printf("Entrées générées: %d\n", entries_found);
    printf("Fichier généré : %s\n", output_name);
}