vmm: vmm.c vmm.h do_request
	cc -o vmm vmm.c vmm.h
do_request:do_request.c vmm.h
	cc -o do_request do_request.c vmm.h
clean:
	rm vmm do_request
