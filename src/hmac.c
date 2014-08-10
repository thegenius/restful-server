#include <stdio.h>
#include <string.h>
#include <openssl/hmac.h>

int main() {
	char key[] = "12345678";
	char data[] = "helloslfjsldfjlsdjfsdjlfjljskdfjsjdfoioejfoajjfdsfsdfjlk";
	unsigned char result[EVP_MAX_MD_SIZE+1];
	unsigned int  reslen;
	HMAC(EVP_sha1(), 
		 key, strlen(key),
		 (unsigned char*)data, strlen(data),
		 result, &reslen);
	printf("%d\n", reslen);
	printf("%s\n", result);	
	printf("%d\n",EVP_MAX_MD_SIZE);
	return 0;
}
