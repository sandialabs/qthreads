#include <stdio.h>

#include "brg_sha1.h"
#include "rng_brg.h"

void rng_init(RNG_state *newstate, int seed)
{
  sha1_ctx ctx;
  struct state_t gen;
  int i;

  for (i=0; i < 16; i++) 
    gen.state[i] = 0;
  gen.state[16] = 0xFF & (seed >> 24);
  gen.state[17] = 0xFF & (seed >> 16);
  gen.state[18] = 0xFF & (seed >> 8);
  gen.state[19] = 0xFF & (seed >> 0);
  
  sha1_begin(&ctx);
  sha1_hash(gen.state, 20, &ctx);
  sha1_end(newstate, &ctx);
}

void rng_spawn(RNG_state *mystate, RNG_state *newstate, int spawnnumber)
{
	sha1_ctx ctx;
	uint8_t  bytes[4];
	
	bytes[0] = 0xFF & (spawnnumber >> 24);
	bytes[1] = 0xFF & (spawnnumber >> 16);
	bytes[2] = 0xFF & (spawnnumber >> 8);
	bytes[3] = 0xFF & spawnnumber;
	
	sha1_begin(&ctx);
	sha1_hash(mystate, 20, &ctx);
	sha1_hash(bytes, 4, &ctx);
	sha1_end(newstate, &ctx);
}

int rng_rand(RNG_state *mystate){
        int r;
	uint32_t b =  (mystate[16] << 24) | (mystate[17] << 16)
		| (mystate[18] << 8) | (mystate[19] << 0);
	b = b & POS_MASK;
	
	r = (int) b;
	return r;
}

int rng_nextrand(RNG_state *mystate){
	sha1_ctx ctx;
	int r;
	uint32_t b;

	sha1_begin(&ctx);
	sha1_hash(mystate, 20, &ctx);
	sha1_end(mystate, &ctx);
	b =  (mystate[16] << 24) | (mystate[17] << 16)
		| (mystate[18] << 8) | (mystate[19] << 0);
	b = b & POS_MASK;
	
	r = (int) b;
	return r;
}

/* condense state into string to display during debugging */
char * rng_showstate(RNG_state *state, char *s){
  sprintf(s,"%.2X%.2X...", state[0],state[1]);
  return s;
}

/* describe random number generator type into string */
int rng_showtype(char *strBuf, int ind) {
  ind += sprintf(strBuf+ind, "SHA-1 (state size = %uB)",
                 (unsigned) sizeof(struct state_t));
  return ind;
}

