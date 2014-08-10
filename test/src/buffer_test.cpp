#include "buffer.hpp"

int main() {
	Buffer<int> buffer;
	int arr1[2] = {1, 2};
	int arr2[3] = {3,4,5};
	buffer.append(arr1, 2);
	buffer.append(arr2, 3);
	int *ptr = buffer.data();
	int size = buffer.size();

	for (int i=0; i<size; ++i) {
		printf("%d\n", ptr[i]);
	}

	Buffer<char> buffer2;
	buffer2.append("sjdflsjf");
	buffer2.append("this is my buffer");
	buffer2.append("slkjdfljsldjflsjdkfjsljdfljsdflsjfdlkjsdf");
	buffer2.append("dsljdfljsldjflsjldfjsdjflsjdjfsljflsjdfjlsjfdljsf 12323\n");
	printf("%s\n", buffer2.data());


	
	return 0;
}
