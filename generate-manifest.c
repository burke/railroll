// Adapted from https://github.com/camlistore/camlistore/blob/master/pkg/rollsum/rollsum.go
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <openssl/sha.h>

const uint32_t windowSize        = 64; // number of bytes to consider in rolling sum
const uint32_t charOffset        = 31; // magic prime number from rsync improves hash
const uint32_t splitBits         = 17; // how many trailing bits must be zero to be considered a split?
const uint32_t splitMask         = (1 << splitBits) - 1; // e.g. 0x0000ffff
const uint32_t maxBlobSize       = 1 << 20; // 1M
const uint32_t tooSmallThreshold = 1 << 16; // 2^16 == 64k

struct RollSum {
  uint32_t s1;
  uint32_t s2;
  uint8_t window[windowSize];
  int wofs;
};

struct RollSum *NewRollSum() {
  struct RollSum *rs = calloc(1, sizeof (struct RollSum));
  rs->s1 = windowSize * charOffset;
  rs->s2 = windowSize * (windowSize - 1) * charOffset;
  return rs;
}

// This optimizes better as a macro than as an inline function ¯\_(ツ)_/¯
#define Roll(rs, add) do { \
  uint8_t drop = (rs)->window[(rs)->wofs]; \
  (rs)->s1 += (add) - drop; \
  (rs)->s2 += ((rs)->s1 - windowSize*(uint8_t)(drop+charOffset)); \
  (rs)->window[(rs)->wofs] = (add); \
  (rs)->wofs = ((rs)->wofs + 1) % windowSize; \
} while (0)

int dumpManifest(char *path) {
  struct RollSum *rs;
  int fd, i, j, start, blobSize;
  uint8_t *data;
  struct stat sbuf;
  bool splitCandidate;

  if ((fd = open(path, O_RDONLY)) == -1) {
    perror("open");
    exit(1);
  }

  if (stat(path, &sbuf) == -1) {
    perror("stat");
    exit(1);
  }

  data = (uint8_t *)mmap((caddr_t)0, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (data == (uint8_t *)(-1)) {
      perror("mmap");
      exit(1);
  }

  rs = NewRollSum();

  blobSize = 0;
  for (i = 0; i < sbuf.st_size; i++) {
    blobSize++;
    Roll(rs, data[i]);
    splitCandidate = ((rs)->s2 & splitMask) == splitMask;
    if (blobSize == maxBlobSize || (splitCandidate && blobSize > tooSmallThreshold)) {
      // If splitting here would make the final chunk too small, don't split.
      if (i > sbuf.st_size - tooSmallThreshold) {
        continue;
      }
      start = (i+1) - blobSize;

      unsigned char hash[SHA_DIGEST_LENGTH];
      char buf[SHA_DIGEST_LENGTH*2];

      SHA1((const unsigned char *)&data[start], blobSize, hash);
      for (j=0; j < SHA_DIGEST_LENGTH; j++) {
        sprintf((char*)&(buf[j*2]), "%02x", hash[j]);
      }

      printf("%d\t%d\t%s\n", start, blobSize, buf);
      blobSize = 0;
    }
  }

  // final blob
  if (blobSize > 0) {
    start = sbuf.st_size - blobSize;
    unsigned char hash[SHA_DIGEST_LENGTH];
    char buf[SHA_DIGEST_LENGTH*2];

    SHA1((const unsigned char *)&data[start], blobSize , hash);
    for (j=0; j < SHA_DIGEST_LENGTH; j++) {
      sprintf((char*)&(buf[j*2]), "%02x", hash[j]);
    }

    printf("%d\t%d\t%s\n", start, blobSize, buf);
    blobSize = 0;
  }

  free(rs);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <path>\n", argv[0]);
    exit(1);
  }

  return dumpManifest(argv[1]);
}

