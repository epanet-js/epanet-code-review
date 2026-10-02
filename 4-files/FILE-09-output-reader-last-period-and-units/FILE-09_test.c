/*
 * FILE-09: the output-file reader cannot reach the last period and
 * misreads pressure units.
 *
 * The test runs FILE-09_kpa.inp (4 hours, 5 reporting periods, pressures in
 * kPa) to write FILE-09.out, then reads that file with the output reader
 * (epanet_output.h). It asks for J1's pressure series over all periods and
 * for the pressure unit code.
 *
 * Correct behaviour: ENR_getNodeSeries(0, nPeriods) returns all 5 values,
 * the last of them equal to the period-4 value from ENR_getNodeAttribute,
 * and ENR_getUnits(ENR_pressUnits) returns ENR_KPA.
 */
#include <stdio.h>
#include "epanet2_2.h"
#include "epanet_output.h"

int main(void)
{
    EN_Project ph;
    ENR_Handle out = NULL;
    float *series = NULL, *values = NULL;
    int err, nPeriods, len = 0, nvalues = 0, unit, lastPeriodLost = 0;

    /* Write the output file */
    EN_createproject(&ph);
    err = EN_open(ph, "FILE-09_kpa.inp", "FILE-09.rpt", "FILE-09.out");
    if (!err) err = EN_solveH(ph);
    if (!err) err = EN_solveQ(ph);
    EN_close(ph);
    EN_deleteproject(ph);
    if (err) {
        printf("FAIL: could not run the network (error %d)\n", err);
        return 1;
    }

    ENR_init(&out);
    if (ENR_open(out, "FILE-09.out") != 0) {
        printf("FAIL: the output reader could not open FILE-09.out\n");
        return 1;
    }
    ENR_getTimes(out, ENR_numPeriods, &nPeriods);
    printf("Reporting periods in the file: %d\n", nPeriods);

    /* 1. The whole series for J1 (node 1); endPeriod is exclusive */
    err = ENR_getNodeSeries(out, 1, ENR_pressure, 0, nPeriods, &series, &len);
    printf("ENR_getNodeSeries(J1, pressure, 0, %d) returned %d, %d values\n",
           nPeriods, err, len);
    if (err) {
        lastPeriodLost = 1;

        /* The widest range the reader accepts */
        err = ENR_getNodeSeries(out, 1, ENR_pressure, 0, nPeriods - 1, &series, &len);
        printf("ENR_getNodeSeries(J1, pressure, 0, %d) returned %d, %d values\n",
               nPeriods - 1, err, len);
        if (!err) ENR_free((void **)&series);
    } else {
        ENR_getNodeAttribute(out, nPeriods - 1, ENR_pressure, &values, &nvalues);
        printf("  last value %.3f, period %d from ENR_getNodeAttribute %.3f\n",
               series[len - 1], nPeriods - 1, values[0]);
        if (len != nPeriods || series[len - 1] != values[0]) lastPeriodLost = 1;
        ENR_free((void **)&values);
        ENR_free((void **)&series);
    }

    /* 2. The pressure unit code: the engine writes 1 for kPa */
    ENR_getUnits(out, ENR_pressUnits, &unit);
    printf("ENR_getUnits(ENR_pressUnits) returned %d: ENR_PSI is %d, ENR_KPA %d, "
           "ENR_MTR %d\n", unit, ENR_PSI, ENR_KPA, ENR_MTR);
    ENR_close(&out);

    if (lastPeriodLost || unit != ENR_KPA) {
        printf("FAIL:");
        if (lastPeriodLost) printf(" the last period cannot be read as a series;");
        if (unit != ENR_KPA) {
            printf(" the kPa file has unit code %d, which the reader calls %s", unit,
                   unit == ENR_MTR ? "ENR_MTR (metres)" : "unknown");
        }
        printf("\n");
        return 1;
    }
    printf("PASS: the reader returns every period and identifies kPa\n");
    return 0;
}
