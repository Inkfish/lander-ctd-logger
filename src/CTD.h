#include <stddef.h>


// The first argument to handle_ctd_input() is a pointer to a function that we
// can call to output the aggregated sample.
typedef size_t (*writefn_t)(const char *str);


/*
Parse a serial string from the Sea-Bird SBE 49 FastCAT CTD.

Only OutputFormat=3 (engineering units in decimal) is supported. Each field is
8 bytes long and left-padded with spaces:

    ttt.tttt, cc.ccccc, pppp.ppp[, sss.ssss][, vvvv.vvv]\n
        '         '         '         '           '- sound velocity (m/s)
        '         '         '         '- salinity (psu)
        '         '         '- pressure (decibars)
        '         '- conductivity (S/m)
        ' temperature (deg C, ITS-90)
*/
void handle_ctd_input(writefn_t writefn, char *buffer, size_t len);
