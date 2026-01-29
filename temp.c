/**
 * @brief The function will parse a string (port) and convert it into a number (also doing validation checks).
 *
 * @param string_port    NULL-terminated string that contains the port in base 10. If NULL or empty, the function will returns @p port_desired_fallback
 * @param port_desired_fallback Port that gets returned when @p string_port does not satisfy requirements
 * @return The parsed port number as an int32_t, or @p port_desired_fallback
 */
static int32_t parse_port_string(const char *string_port, int32_t port_desired_fallback)
{
    if (string_port == NULL || string_port[0] == '\0') // If the string is empty or a null pointer
    {
        return port_desired_fallback;
    }

    char *endptr = NULL;
    errno = 0; // reset errno so we do not get false alarms
    long port_long = strtol(string_port, &endptr, 10); // convert string port to long
    if (endptr == string_port || *endptr != '\0' || errno != 0 || port_long < 0 || port_long > 65535)
    {
        P("Invalid port string_port [%s], using fallback: %d", string_port, port_desired_fallback);
        return port_desired_fallback;
    }
    if (port_long > 0 && port_long < 1024) // If the chosen port is reserved
    {
        P(" The Port %ld is probably a reserved port on UNIX systems, defaulting to ephemeral", port_long);
        return 0;
    }
    return (int32_t)port_long; // Conversion to silence gcc
}