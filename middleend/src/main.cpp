#include "utils/statuses.h"
#include "utils/args_parser.h"
#include "log/log.h"
#include "midend.h"

LogFileData log_file = {"log"};

/**
 * @brief This array contains console options, their functions and descriptions
 */
const Argument ARGS_DICT[] = {
    {"-h", print_help,           "#   -h - prints help information\n"},             //< Help option

    {"-i", read_input_filename,  "#   -i - specify input file name after this\n"},  //< Input filename

    {"-o", read_output_filename, "#   -o - specify output file name after this\n"}, //< Output filename
};

const int ARGS_DICT_LEN = sizeof(ARGS_DICT) / sizeof(ARGS_DICT[0]);                 //< args_dict array len

int main(int argc, char* argv[]) {

    /// Parsing console arguments
    ArgsVars args_vars = {};
    args_vars.input_filename  = "Programs/main/prog.tre"; //< default value
    args_vars.output_filename = "Programs/main/prog.tre"; //< default value

    STATUS_CHECK_RAISE(args_parse(argc, argv, &args_vars, ARGS_DICT, ARGS_DICT_LEN));
    /// Parsing console arguments end

    STATUS_CHECK_RAISE(mid_process(args_vars.input_filename, args_vars.output_filename));

    return Status::OK_EXIT;
}
