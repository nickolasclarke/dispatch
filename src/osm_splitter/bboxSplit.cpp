//https://github.com/openstreetmap/gosmore/blob/master/bboxSplit.cpp
//Modified from commit: 7e2acfb2d586acb147b27d442e8fac3c2199eb50
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <vector>
#include <map>
#include <assert.h>



struct younion { // Union of 1 or more bboxes. Terminated with -1.
  int *i;
  younion (int *_i) : i (_i) {}
};



bool operator < (const younion &a, const younion &b)
{
  int *ap = a.i, *bp;
  for (bp = b.i; *bp == *ap && *bp != -1; bp++) ap++;
  return *ap < *bp;
}



struct BoxCoordinates {
  double minlat, minlon, maxlat, maxlon;
  BoxCoordinates(double minlat, double minlon, double maxlat, double maxlon) :
    minlat(minlat), minlon(minlon), maxlat(maxlat), maxlon(maxlon) {}
};



int main (int argc, char *argv[]){
  std::vector<char> buf(1000*1024*1024); //Largest object is assumed to be <1000MB

  const int bcnt = (argc - 1) / 6;
  std::vector<BoxCoordinates> box_coordinates;
  std::vector<FILE *> f(bcnt);
  if (argc <= 1 || argc % 6 != 1) {
    fprintf (stderr, "Usage: %s bottom left top right pname fname [...]\n"
      "Reads an OSM-XML file from standard in and cut it into the given rectangles.\n"
      "pname is exectuted for each rectangle and the XML is piped to it. It's output\n"
      "is redirected to 'fname'. %s does not properly implement job control, but\n"
      "gzip, bzip and cat are acceptable values for pname\n" , argv[0], argv[0]);
    return 1;
  }
  for (int i = 0; i < bcnt; i++) {
    //Open file to hold this box
    FILE *out = fopen (argv[i*6+6], "w");
    dup2 (fileno (out), STDOUT_FILENO);
    f[i] = popen (argv[i*6+5], "w");
    assert (f[i]);
    fclose (out);

    fprintf (f[i], "<?xml version='1.0' encoding='UTF-8'?>\n"
      "<osm version=\"0.6\" generator=\"bboxSplit %s\">\n"
      "<bound box=\"%s,%s,%s,%s\"" 
      /* origin=\"http://www.openstreetmap.org/api/0.6\" */ "/>\n" , __DATE__,
      argv[i * 6 + 1],  argv[i * 6 + 2], argv[i * 6 + 3], argv[i * 6 + 4]);
    
    //Read box coordinates into memory
    box_coordinates.emplace_back(
      std::stod(argv[i * 6 + 0 + 1]), //minlat
      std::stod(argv[i * 6 + 1 + 1]), //minlon
      std::stod(argv[i * 6 + 2 + 1]), //maxlat
      std::stod(argv[i * 6 + 3 + 1])  //maxlon
    );
  }
  std::vector<int*> areas;
  // This vector maps area ids to a list of bboxes and 'amap' maps a list
  // of bboxes back to the id.
  areas.push_back (new int[1]); // Tiny once off memory leak.
  areas.back ()[0] = -1; // Make 0 the empty area
  std::map<younion,int> amap;
  amap[younion (areas.back ())] = 0;
  
  areas.push_back (new int[bcnt + 1]); // Tiny once off memory leak.
  areas.back ()[0] = -1; // Always have an empty set ready.
  
  typedef unsigned short areasIndexType;
  std::vector<areasIndexType> nwr[3]; // Nodes, relations, ways
  char *start = buf.data();
  long tipe[10];
  long id=0;
  long olevel = 0;
  long memberTipe = 0;
  long ref=0;
  long acnt = 0;
  long level;
  double lat=0;
  double lon=0;
  for (int cnt = 0, i; (i = fread (buf.data() + cnt, 1, buf.size() - cnt, stdin)) > 0;) {
    cnt += i;
    char *ptr = start;
    char *n;
    level = olevel;
    do {
      //printf ("-- %d %.20s\n", level, ptr);
      int isEnd = (ptr + 1 < buf.data() + cnt) &&
        ((ptr[0] == '<' && ptr[1] == '/') || (ptr[0] == '/' && ptr[1] == '>'));
      for (n = ptr; n < buf.data() + cnt &&
                    (isEnd ? *n != '>' : !isspace (*n) && *n != '/'); n++) {
        if (*n == '\"') {
          for (++n; n < buf.data() + cnt && *n != '\"'; n++) {}
        }
        else if (*n == '\'') {
          for (++n; n < buf.data() + cnt && *n != '\''; n++) {}
        }
      }
      if (isEnd && n < buf.data() + cnt) n++; // Get rid of the '>'
      while (n < buf.data() + cnt && isspace (*n)) n++;
      
      if (isEnd && level == 2 && tipe[level - 1] == 'o') { // Note: n may be at buf.data() + cnt
        nwr[0].clear (); // Free some memory for in case one or more
        nwr[1].clear (); // processes does heavy postprocessing.
        nwr[2].clear ();
        for (int j = 0; j < bcnt; j++) fprintf (f[j], "</osm>\n");
        // By splitting these two steps we allow downstream XML converters
        // like gosmore to do their post-XML processing in parallel.
        for (int j = 0; j < bcnt; j++) pclose (f[j]);
        
        // Should we close the files and wait for the children to exit ?
        fprintf (stderr, "%s done using %ld area combinations\n", argv[0], areas.size () - 1);
        return 0;
      }
      if (n >= buf.data() + cnt) {}
      else if (isEnd) {
        //printf ("Ending %c at %d\n", tipe[level - 1], level);
        if (--level == 2 && tipe[level] == 'n') { // End of a node
          for (int j = 0; j < bcnt; j++) {
            if (box_coordinates[j].minlat < lat && box_coordinates[j].minlon < lon && lat < box_coordinates[j].maxlat && lon < box_coordinates[j].maxlon) {
              areas.back ()[acnt++] = j;
            }
          }
          areas.back ()[acnt] = -1;
        }
        else if ((tipe[level] == 'n' || tipe[level] == 'm')
                 && level == 3) { // End of an '<nd ..>' or a '<member ...>
          memberTipe = tipe[2] == 'w' || memberTipe == 'n' ? 0
                       : memberTipe == 'w' ? 1 : 2;
          if (ref < (long int)nwr[memberTipe].size ()) {
            for (int j = 0, k = 0; areas[nwr[memberTipe][ref]][j] != -1; j++) {
              while (k < acnt && areas.back()[k] < areas[nwr[memberTipe][ref]][j]) k++;
              if (k >= acnt || areas.back()[k] > areas[nwr[memberTipe][ref]][j]) {
                memmove (&areas.back()[k + 1], &areas.back()[k],
                  sizeof (areas[0][0]) * (acnt++ - k));
                areas.back()[k] = areas[nwr[memberTipe][ref]][j];
              }
            } // Merge the two lists
          }
        }
        if (level == 2 && acnt > 0) { // areas.back()[0] != -1) {
        //(tipe[2] == 'n' || tipe[2] == 'w' || tipe[2] == 'r')) { // not needed for valid OSM-XML
          for (int j = 0; j < acnt /* areas.back()[j] != -1*/; j++) {
            //assert (areas.back ()[j] < bcnt);
            fwrite (start, 1, n - start, f[areas.back()[j]]);
          }
          areas.back ()[acnt] = -1;
          std::map<younion,int>::iterator mf = amap.find (younion (areas.back()));
          if (mf == amap.end ()) {
            int pos = areas.size () - 1;
            if (pos >> (sizeof (areasIndexType) * 8)) {
              for (int j = 0; j < bcnt; j++) fprintf (f[j], "</osm>\n");
              // By splitting these two steps we allow downstream XML converters
              // like gosmore to do their post-XML processing in parallel.
              for (int j = 0; j < bcnt; j++) pclose (f[j]);
              fprintf (stderr, "%s FATAL: Too many combinations of areas\n", argv[0]);
              return 2;
            }
            amap[younion (areas.back ())] = pos;
            mf = amap.find (younion (areas.back()));
            areas.push_back (new int[bcnt + 1]); // Tiny once off memory leak.
            //assert (f != amap.end());
          }
          int nwrIdx = tipe[2] == 'n' ? 0 : tipe[2] == 'w' ? 1 : 2;
          //printf (stderr, "Extending %c to %ld\n", tipe[2], id);
          while ((long int)nwr[nwrIdx].size () <= id) nwr[nwrIdx].push_back (0);
          // Initialize nwr with 0 which implies the empty union
          nwr[nwrIdx][id] = mf->second;
          areas.back ()[0] = -1;
          acnt = 0;
        } // if we found an entity that belongs to at least 1 bbox
        if (level == 2) {
          start = n;
          olevel = level;
        }
      } // If it's /> or </..>
      else if (*ptr == '<') {
        if (ptr[1] != '!') tipe[level++] = ptr[1];
      }
      // The tests for 'level' is not necessary for valid OSM-XML
      else if (level == 3 && strncasecmp(ptr, "id=", 3) == 0) {
        id = std::stoll(ptr[3] == '\'' || ptr[3] == '\"' ? ptr + 4 : ptr + 3);
      }
      else if (level == 3 && strncasecmp(ptr, "lat=", 4) == 0) {
        lat = std::stof(ptr[4] == '\'' || ptr[4] == '\"' ? ptr + 5 : ptr + 4);
      }
      else if (level == 3 && strncasecmp(ptr, "lon=", 4) == 0) {
        lon = std::stof(ptr[4] == '\'' || ptr[4] == '\"' ? ptr + 5 : ptr + 4);
      }
      else if (level == 4 && strncasecmp(ptr, "type=", 4) == 0) {
        memberTipe = ptr[5] == '\'' || ptr[5] == '\"' ? ptr[6] : ptr[5];
      }
      else if (level == 4 && strncasecmp(ptr, "ref=", 4) == 0) {
        ref = std::stoll(ptr[4] == '\'' || ptr[4] == '\"' ? ptr + 5 : ptr + 4);
      }
      ptr = n;
    } while (ptr + 1 < buf.data() + cnt);
    memmove (buf.data(), start, buf.data() + cnt - start);
    cnt -= start - buf.data();
    start = buf.data();
  }
  for (int j = 0; j < bcnt; j++) fprintf (f[j], "</osm>\n");
  // By splitting these two steps we allow downstream XML converters
  // like gosmore to do their post-XML processing in parallel.
  for (int j = 0; j < bcnt; j++) pclose (f[j]);
  fprintf (stderr, "Warning: Xml termination not found. Files should be OK.\n");
  fprintf (stderr, "%s done using %ld area combinations\n", argv[0], areas.size () - 1);
  return 1;
}
