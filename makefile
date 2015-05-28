vmm: vmm.c vmm.h do_request give_request
	cc -o vmm vmm.c vmm.h
do_request:do_request.c vmm.h
	cc -o do_request do_request.c vmm.h
give_request:give_request.c vmm.h
	cc -o give_request give_request.c vmm.h
clean:
	rm give_request vmm do_request
