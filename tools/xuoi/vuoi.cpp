#include <stdio.h>
#include <inttypes.h>
#include <xuocore/http.h>
#include <xuocore/uo_installer.h>

int main(int argc, char **argv)
{
    http_init();
    uoi_load_contents();
}