// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The Android Auto head unit identity shown to the phone in TLS: certificate and private key (data only).
#ifndef ENTRY_AAD_CORE_INCLUDE_HU_CERTIFICATE_H
#define ENTRY_AAD_CORE_INCLUDE_HU_CERTIFICATE_H

// Signed by the Google Automotive Link CA, valid until 2048.
static const char* const kHeadUnitCertificatePem =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDPzCCAicCAgERMA0GCSqGSIb3DQEBCwUAMFsxCzAJBgNVBAYTAlVTMRMwEQYD\n"
    "VQQIDApDYWxpZm9ybmlhMRYwFAYDVQQHDA1Nb3VudGFpbiBWaWV3MR8wHQYDVQQK\n"
    "DBZHb29nbGUgQXV0b21vdGl2ZSBMaW5rMCYXETE0MDcwNDAwMDAwMC0wNzAwFxE0\n"
    "ODA4MDExMDIxMjMtMDcwMDBnMQswCQYDVQQGEwJVUzETMBEGA1UECAwKQ2FsaWZv\n"
    "cm5pYTEWMBQGA1UEBwwNTW91bnRhaW4gVmlldzEeMBwGA1UECgwVQW5kcm9pZC1B\n"
    "dXRvLUludGVybmFsMQswCQYDVQQLDAIwMTCCASIwDQYJKoZIhvcNAQEBBQADggEP\n"
    "ADCCAQoCggEBAOWghAac2eJLbi/ijgZGRB6/MuaBVfOImkQddBJUhXbnskTJB/JI\n"
    "12Ea22E5GeVN8CkWULAZT28yDWqsKMyq9BzpjpsHc9TKxMYqrIn0HP7mIJcBu5z7\n"
    "K8DoXqc86encncJlkGeuQkUA68yyp7RG7eQ6XoBHEjNmyvX13Y8NY5sPUHfLfmp6\n"
    "A2n+Jdmecq3L0GS84ctdNtnp2zSopTy0L1Gp6+lrnuOPAYZeV+Ei2jAvhycvuSoB\n"
    "yV6rT9wvREvC2TDncurMwR6ws44+ZStqkhnvDLhV04ray5aPplQwwB9GELFCYSRk\n"
    "56sm57uYSJj/LlmOMcvyBmUHVJ7MLxgtlykCAwEAATANBgkqhkiG9w0BAQsFAAOC\n"
    "AQEA1Bs8v6HuAIiBdhGDGHzZJDwO6lW0LheBqsGLG9KsVvIVrTMPP9lpdTPjStGn\n"
    "en1RIce4R4l3YTBwxOadLMkf8rymAE5JNjPsWlBue7eI4TFFw/cvnKxcTQ61bC4i\n"
    "2uosyDI5VfrXm38zYcZoK4TFtMhNyx6aYSEClWB9MjHa+n6eR3dLBCg1kMGqGdZ/\n"
    "AoK0UEkyI3UFU8sW86iaS4dvPSaQ+z0tmfUzbrc5ZSk4hYCeUYvuyd2ShxjKmxvD\n"
    "0K8A7gKLY0jP8Zp+6rYBcpxc7cylWMbdlhFTHAGiKI+XeQ/9u+RPeocZsn5jGlDt\n"
    "K3ftMoWFce+baNq/WcMzRj04AA==\n"
    "-----END CERTIFICATE-----\n";

static const char* const kHeadUnitPrivateKeyPem =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDloIQGnNniS24v\n"
    "4o4GRkQevzLmgVXziJpEHXQSVIV257JEyQfySNdhGtthORnlTfApFlCwGU9vMg1q\n"
    "rCjMqvQc6Y6bB3PUysTGKqyJ9Bz+5iCXAbuc+yvA6F6nPOnp3J3CZZBnrkJFAOvM\n"
    "sqe0Ru3kOl6ARxIzZsr19d2PDWObD1B3y35qegNp/iXZnnKty9BkvOHLXTbZ6ds0\n"
    "qKU8tC9Rqevpa57jjwGGXlfhItowL4cnL7kqAcleq0/cL0RLwtkw53LqzMEesLOO\n"
    "PmUrapIZ7wy4VdOK2suWj6ZUMMAfRhCxQmEkZOerJue7mEiY/y5ZjjHL8gZlB1Se\n"
    "zC8YLZcpAgMBAAECggEANfN+0abssnSSkQGj6Ol9e9Pke7ALp8qCpFpfOSdOrF9H\n"
    "zU2GZnCi2hfbT36BDRVAxgaOMrHfrfqOmG+Wrm/EJz7g2J/poSd7ECxamZcXcPRU\n"
    "1+X6VKnW3944zwZK1z2Ue2YthY6110yeGlwndv3rSuJD7dPE/Pq72QpJYiD9NCL1\n"
    "DoX98hDpl570C8bpD/8I4lgD2YqGcLmR6cHaKx0gQjxfr2gPY22uu0rDsriTPUQX\n"
    "og01iG7iwbFZkGD0/N0LdBHQtfXclDWZvi3sm/1Td2d1GmKw5QGl69BAxNlyrpo9\n"
    "TCYIXcop5/ZJTvoJ06wxKBmimYtWJZMuKlC6eDg9AQKBgQD1RIj4fkpuL1D5Cj1b\n"
    "ceNUNAcsRNuDNZmo3WvDVBNVGQaJKQcm0EzUwc94UKuWZ/lTBOJyBYUbdsiQTHsd\n"
    "uHk0bUmP1II7/k7cieP4sGGZEV9ouB6PC8N3wPryncyBQdRAUsoaOsWu/bzAP08w\n"
    "q08LxwtweUiTzubPjnwKdBUV0QKBgQDvrMZsHmn6PLIaC1kpvWLeIAC4ZHOJffD8\n"
    "++ap5djTDLP6tmve7eZVL1XuVhsPag5JlBs1bPenvqcTMTqP4my33W6GSXz8dqHq\n"
    "X271tY7Zw+pf1rjTYZ8khufrdMia5zSAKTZ27hofVobDJWDJwo+ijQLcIwwkVN3K\n"
    "COqdU53J2QKBgGP8jY4oBfXHSk5MU1kdI5ynHSl55Lcnub2yjoTxmHP/oL93v1ee\n"
    "YUP2wrp8wIavWl/yxNHQ2EOq9/aDh0lFc/vgdbWx32nPsZyMhihS12bMQ0XwkVyl\n"
    "YglG3cbcd4tWJ84VMhjhTseY30LMTF895xqZCVvPxbukBEom/wBP9GyRAoGAXSR6\n"
    "9DeZnesKl/6DByqXipVLUl2PXQZ5qgdHLA2sBtDv/OX54+f2VHKR5Y7G82WW6ddJ\n"
    "/6g6UpFQIrBF/vbSo13FfyB+FysPX6L43zpQqqZtphMvPs64ZxyacrGu6CwkBg5+\n"
    "IRgFRNp+gsY66A3ZfiVo0B46sL/edY6jhsKhFnkCgYEAqHKJM0GrxITZYLMjCXZC\n"
    "udZvONe14MnYUJfxpQ3CLgpBq5QbTQq8ITtVAvd7PHrdHzW4B4+yKroTQ2gn6ev0\n"
    "nTFBkYuHpa2CvVSumc30qfZpeKMatdmbQC2FVEPSOQS5cx6B7YYS0nW9nRon+Sbt\n"
    "GQ5XKUTGTaUvJR1gYDDMrlM=\n"
    "-----END PRIVATE KEY-----\n";

#endif
