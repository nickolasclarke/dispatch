/*

  EXAMPLE osmium_convert

  Convert OSM files from one format into another.

  DEMONSTRATES USE OF:
  * file input and output
  * file types
  * Osmium buffers

  SIMPLER EXAMPLES you might want to understand first:
  * osmium_read

  LICENSE
  The code in this example file is released into the Public Domain.

*/

#include <exception>
#include <fstream>
#include <iostream>
#include <string>

// Allow any format of input files (XML, PBF, ...)
#include <osmium/io/any_input.hpp>

// Allow any format of output files (XML, PBF, ...)
#include <osmium/io/any_output.hpp>

#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/io/reader_with_progress_bar.hpp>



struct BoxCoordinates {
  double minlat, minlon, maxlat, maxlon;
  BoxCoordinates(double minlat, double minlon, double maxlat, double maxlon) :
    minlat(minlat), minlon(minlon), maxlat(maxlat), maxlon(maxlon) {}
  bool contains(const double lat, const double lon) const {
    return minlat<lat && lat<maxlat && minlon<lon && lon<maxlon;
  }
};



// This handler only implements the way() function, we are not interested in
// any other objects.
class Boxer : public osmium::handler::Handler {
 private:
  std::vector<BoxCoordinates> box_coordinates;
  std::vector<std::shared_ptr<osmium::io::Writer>> writers;

 public:
  Boxer(const std::string &box_filename, const osmium::io::Header &header, const double margin){
    std::ifstream fboxin(box_filename);
    double minlat, minlon, maxlat, maxlon;
    std::string filename;
    while(fboxin>>minlat>>minlon>>maxlat>>maxlon>>filename){
      std::cout<<"filename: '"<<filename<<"' with box: "<<minlat<<","<<minlon<<"  "<<maxlat<<","<<maxlon<<std::endl;

      const osmium::io::File output_file{filename};

      // Initialize Writer using the header from above and tell it that it
      // is allowed to overwrite a possibly existing file.
      writers.emplace_back(new osmium::io::Writer{output_file, header, osmium::io::overwrite::allow});

      //Read box coordinates into memory
      box_coordinates.emplace_back(minlat-margin,minlon-margin,maxlat+margin,maxlon+margin);
      std::cout<<"Got box: "<<minlat<<","<<minlon<<"  "<<maxlat<<","<<maxlon<<std::endl;
    }
  }

  void node(osmium::Node& node) {
    const auto nlat = node.location().lat();
    const auto nlon = node.location().lon();
    //Could parallelize, but probably IO bound.
    for(unsigned int i=0;i<box_coordinates.size();i++){
      if(box_coordinates[i].contains(nlat,nlon)){
        (*writers[i])(node);
      }
    }
  }

  // If the way has a "highway" tag, find its length and add it to the
  // overall length.
  void way(const osmium::Way& way) {
    //Could parallelize, but probably IO bound.
    for(unsigned int i=0;i<box_coordinates.size();i++){
      for(const auto &n: way.nodes()){
        if(box_coordinates[i].contains(n.lat(),n.lon())){
          (*writers[i])(way);
          break;
        }
      }
    }
  }

  void close() {
    for(auto &w: writers){
      w->close();
    }
  }
};



int main (int argc, char *argv[]){
  if (argc != 4) {
    std::cerr<<"Usage: "<<argv[0]<<" <osm highway file> <boxes> <margin>\n"
      "Reads an OSM-PBF file and cuts it into the given rectangles.\n"
      "The boxes file must have the form\n"
      "  <minlat> <minlon> <maxlat> <maxlon> <filename>\n"
      "  [...]\n"
      "Rectangles are specified equivalently by: (bottom left top right)\n"
      "Margin is a decimal number indicating how many degrees margin to add to the box\n"
      "Remember: 1 degree of latitude is equal to 69 miles, so a 1 mile margin is 0.0144 degrees"
      <<std::endl;
    return 1;
  }

  const std::string osm_filename = argv[1];
  const std::string box_filename = argv[2];
  const double margin = std::stod(argv[3]);

  // Initialize Reader
  osmium::io::ReaderWithProgressBar reader{true, osm_filename, osmium::osm_entity_bits::node | osmium::osm_entity_bits::way};

  //Get header from input file and change the "generator" setting to ourselves.
  osmium::io::Header header = reader.header();
  header.set("generator", "osm_splitter");

  Boxer boxer(box_filename, header, margin);

  using index_type = osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type, osmium::Location>;
  using location_handler_type = osmium::handler::NodeLocationsForWays<index_type>;

  index_type index;
  location_handler_type location_handler{index};

  osmium::apply(reader, location_handler, boxer);

  reader.close();
  boxer.close();

  return 0;
}
