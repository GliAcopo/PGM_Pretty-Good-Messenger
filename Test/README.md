# Test Input Files for `bin/client`

### How to run the tests?
Build the client (`make client`), start the server on the target host/port when needed, then run for example:

```bash
./bin/client < Test/valid_registration_new_user.txt
```

Each line corresponds to what you would type at the prompts (username, confirmation, server IP, port, passwords).

- `valid_registration_new_user.txt` - New user path; confirms username, connects to `127.0.0.1:666`, registers with `TestPassword123!`.
- `valid_login_existing_user.txt` - Existing user path using `existing_user` and password `CorrectHorseBatteryStaple`.
- `invalid_ip_address.txt` - Uses a bogus IP (`999.999.999.999`); should fail before connect.
- `invalid_port_non_numeric.txt` - Supplies `abc` for the port; exercises non-numeric handling.
- `invalid_port_out_of_range.txt` - Supplies `70000` as the port; exercises out-of-range rejection.
- `username_retry_flow.txt` - First username rejected (`n`), then accepts `clean_user`; continues to `127.0.0.1:666`.
- `blank_password_registration.txt` - Registers `blank_pwd_user` with an intentionally empty password.
- `wrong_password_three_attempts.txt` - Existing user path with three wrong passwords to hit the max-attempt logic.
