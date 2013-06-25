/**
 *  dostimes.c
 *
 *  Implements functions for converting times between DOS and UNIX formats.
 *
 *  Author: Matthew Signorini
 */

#include "const.h"
#include "dostimes.h"


// constants used for calculating times in seconds.
#define SECONDS_PER_MINUTE          60
#define MINUTES_PER_HOUR            60
#define HOURS_PER_DAY               24

#define SECONDS_PER_HOUR            (MINUTES_PER_HOUR * SECONDS_PER_MINUTE)
#define SECONDS_PER_DAY             (HOURS_PER_DAY * SECONDS_PER_HOUR)


// local functions.
PRIVATE unsigned int days_since_epoch (unsigned int year);
PRIVATE unsigned int days_before (unsigned int month, unsigned int year);
PRIVATE unsigned int days_this_month (unsigned int month, unsigned int year);


/**
 *  Translate a DOS date and time pair into UNIX time.
 */
    PUBLIC time_t
unix_time (date, time)
    unsigned int date;      // DOS date.
    unsigned int time;      // DOS time.
{
    time_t utime = 0;

    // first, we will deal with the date. We need to get the number of
    // days since the UNIX epoch, then we will convert that into a
    // number of seconds.
    utime = days_since_epoch (DATE_YEAR (date));
    utime += days_before (DATE_MONTH (date), DATE_YEAR (date));
    utime += DATE_DAYS (date);

    // and we know how many seconds are in a day, of course.
    utime *= SECONDS_PER_DAY;

    // now add on the time, converted into the number of seconds since
    // 00:00:00.
    utime += (TIME_HOUR (time) * SECONDS_PER_HOUR) + 
        (TIME_MINUTE (time) * SECONDS_PER_MINUTE) + TIME_SECOND (time);

    return utime;
}

/**
 *  Returns the number of days from the UNIX epoch up until the start of
 *  this year.
 */
    PRIVATE unsigned int
days_since_epoch (year)
    unsigned int year;      // current year.
{
    unsigned int days = 0;

    // step the year back to 1970, and add up all the days in each year
    // as we go.
    for ( ; year >= 1970; year -= 1)
    {
        // add the number of days before the end of december to the total.
        days += days_before (13, year);
    }

    return days;
}

/**
 *  Returns the number of days from the start of the year up to the
 *  start of the current month.
 */
    PRIVATE unsigned int
days_before (month, year)
    unsigned int month;     // current month.
    unsigned int year;      // needed to handle leap years.
{
    unsigned int total_days = 0;

    // don't include the current month's days in the total.
    month -= 1;

    // step month back to the start of the year, and add up the days in
    // each month. Note that this uses the arument in the stack, which is
    // a little terse...
    for ( ; month > 0; month -= 1)
    {
        total_days += days_this_month (month, year);
    }

    return total_days;
}

/**
 *  Returns the number of days in a given month.
 */
    PRIVATE unsigned int
days_this_month (month, year)
    unsigned int month;     // month, where january is 1.
    unsigned int year;      // needed to handle leap years.
{
    unsigned int days;

    if ((month == 4) || (month == 6) || (month == 9) || (month == 11))
    {
        // 30 days hath september, april june and november.
        days = 30;
    }
    else if (month == 2)
    {
        // february has 28 days, or 29 on leap years.
        if (((year % 4) == 0) && (((year % 100) != 0) || 
              ((year % 400) == 0)))
        {
            // leap year.
            days = 29;
        }
        else
        {
            days = 28;
        }
    }
    else
    {
        // all the rest have 31.
        days = 31;
    }

    return days;
}


// vim: ts=4 sw=4 et
