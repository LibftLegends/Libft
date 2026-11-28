#include "../../../Modules/CSV/csv.hpp"
#include <cinttypes>
#include <cstdio>

static void checksum_number(uint64_t &checksum, uint64_t value)
{
    checksum ^= value;
    checksum *= 1099511628211ULL;
    return ;
}

static void checksum_string(uint64_t &checksum, const char *text, ft_size_t length)
{
    checksum_number(checksum, length);
    ft_size_t index = 0U;
    while (index < length)
    {
        checksum_number(checksum, static_cast<uint8_t>(text[index]));
        index += 1U;
    }
    return ;
}

static int32_t check_input(const char *input, char delimiter, uint64_t &checksum)
{
    ft_csv_document document;
    const int32_t document_error = document.initialize(input, delimiter, FT_TRUE);
    checksum_number(checksum, static_cast<uint32_t>(document_error));
    if (document_error == FT_ERR_SUCCESS)
    {
        const ft_size_t row_count = document.row_count();
        checksum_number(checksum, row_count);
        ft_size_t row_index = 0U;
        while (row_index < row_count)
        {
            ft_size_t column_count = 0U;
            const ft_string *row = document.get_row(row_index, &column_count);
            if (row == ft_nullptr)
                return (1);
            checksum_number(checksum, column_count);
            ft_size_t column_index = 0U;
            while (column_index < column_count)
            {
                checksum_string(checksum, row[column_index].c_str(), row[column_index].size());
                column_index += 1U;
            }
            row_index += 1U;
        }
        ft_string encoded;
        if (document.write_to_string(encoded) != FT_ERR_SUCCESS)
            return (1);
        checksum_string(checksum, encoded.c_str(), encoded.size());
    }
    ft_vector<ft_string> fields;
    const int32_t split_error = csv_split_line(input, fields, delimiter);
    checksum_number(checksum, static_cast<uint32_t>(split_error));
    const ft_size_t field_count = fields.size();
    checksum_number(checksum, field_count);
    ft_size_t field_index = 0U;
    while (field_index < field_count)
    {
        const ft_string &field = fields[field_index];
        checksum_string(checksum, field.c_str(), field.size());
        field_index += 1U;
    }
    char *escaped = csv_escape_field(input, delimiter);
    if (escaped == ft_nullptr)
        return (1);
    checksum_string(checksum, escaped, ft_strlen_size_t(escaped));
    cma_free(escaped);
    return (0);
}

int32_t main()
{
    constexpr char ALPHABET[] = {'x', ',', '"', ' ', '\r', '\n', '\t'};
    constexpr char DELIMITERS[] = {',', ';', '\t'};
    constexpr uint32_t MAX_LENGTH = 5U;
    char input[MAX_LENGTH + 1U] = {};
    uint64_t checksum = 14695981039346656037ULL;
    uint32_t total_inputs = 0U;
    uint32_t combination_count = 1U;
    uint32_t length = 0U;
    while (length <= MAX_LENGTH)
    {
        uint32_t combination = 0U;
        while (combination < combination_count)
        {
            uint32_t character_index = 0U;
            uint32_t digits = combination;
            while (character_index < length)
            {
                input[character_index] = ALPHABET[digits % sizeof(ALPHABET)];
                digits /= sizeof(ALPHABET);
                character_index += 1U;
            }
            input[length] = '\0';
            uint32_t delimiter_index = 0U;
            while (delimiter_index < sizeof(DELIMITERS))
            {
                if (check_input(input, DELIMITERS[delimiter_index], checksum) != 0)
                    return (1);
                delimiter_index += 1U;
                total_inputs += 1U;
            }
            combination += 1U;
        }
        combination_count *= sizeof(ALPHABET);
        length += 1U;
    }
    std::printf("%" PRIu32 " inputs, checksum %" PRIu64 "\n", total_inputs, checksum);
    return (0);
}
