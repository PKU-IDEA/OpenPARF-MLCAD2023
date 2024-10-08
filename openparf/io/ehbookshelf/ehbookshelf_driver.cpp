// $Id: driver.cc 39 2008-08-03 10:07:15Z tb $
/** \file driver.cc Implementation of the example::Driver class. */

#include "ehbookshelf_driver.h"

#include "ehbookshelf_scanner.h"

namespace ehbookshelfparser {

Driver::Driver(EHBookshelfDatabase &db)
    : trace_scanning(false),
      trace_parsing(false),
      parser(NULL),
      scanner(NULL),
      db_(db) {
}

Driver::~Driver() {
  delete scanner;
  scanner = NULL;
  delete parser;
  parser = NULL;
}

bool Driver::parse_stream(std::istream &in, std::string const &sname) {
  streamname = sname;

  delete scanner;
  scanner = new Scanner(&in);
  scanner->set_debug(trace_scanning);

  delete parser;
  parser = new Parser(*this);
  parser->set_debug_level(trace_parsing);
  return (parser->parse() == 0);
}

bool Driver::parse_file(std::string const &filename) {
  std::ifstream in(filename.c_str());
  if (!in.good()) return false;
  return parse_stream(in, filename);
}

bool Driver::parse_string(std::string const &input, std::string const &sname) {
  std::istringstream iss(input);
  return parse_stream(iss, sname);
}

void Driver::error(const class location &l, std::string const &m) {
  std::cerr << l << ": " << m << std::endl;
}

void Driver::error(std::string const &m) {
  std::cerr << m << std::endl;
}

//////////////////////////////////
//                              //
//  Parser Call Back Functions  //
//                              //
//////////////////////////////////

void Driver::addCellCbk(std::string const &name) {
  db_.addCellCbk(name);
}
void Driver::addCellInputPinCbk(std::string const &pinName) {
  db_.addCellInputPinCbk(pinName);
}
void Driver::addCellOutputPinCbk(std::string const &pinName) {
  db_.addCellOutputPinCbk(pinName);
}
void Driver::addCellClockPinCbk(std::string const &pinName) {
  db_.addCellClockPinCbk(pinName);
}
void Driver::addCellCtrlPinCbk(std::string const &pinName) {
  db_.addCellCtrlPinCbk(pinName);
}
void Driver::addCellCtrlSRPinCbk(std::string const &pinName) {
  db_.addCellCtrlSRPinCbk(pinName);
}
void Driver::addCellCtrlCEPinCbk(std::string const &pinName) {
  db_.addCellCtrlCEPinCbk(pinName);
}
void Driver::addCellInputCasPinCbk(std::string const &pinName) {
  db_.addCellInputCasPinCbk(pinName);
}
void Driver::addCellOutputCasPinCbk(std::string const &pinName) {
  db_.addCellOutputCasPinCbk(pinName);
}
void Driver::addCellParameterCbk(std::string const &param) {
  db_.addCellParameterCbk(param);
}
void Driver::setSiteTypeCbk(std::string const &str) {
  site_type_ = str;
}
void Driver::setSiteNumResourcesCbk(unsigned n) {
  site_resources_.emplace_back(resource_type_, n);
}
void Driver::endSiteBlockCbk() {
  db_.setSiteResources(site_type_, site_resources_);
  site_resources_.clear();
}
void Driver::setResourceTypeCbk(std::string const &str) {
  resource_type_ = str;
}
void Driver::addToCellNameListCbk(std::string const &str) {
  cell_name_list_.push_back(str);
}
void Driver::addResourceTypeCbk() {
  db_.addResourceTypeCbk(resource_type_, cell_name_list_);
  resource_type_.clear();
  cell_name_list_.clear();
}
void Driver::endResourceTypeBlockCbk() {
  db_.endResourceTypeBlockCbk();
}
void Driver::initSiteMapCbk(unsigned w, unsigned h) {
  db_.initSiteMapCbk(w, h);
}
void Driver::endSiteMapCbk() {
  db_.endSiteMapCbk();
}
void Driver::setSiteMapEntryCbk(unsigned x, unsigned y, std::string const &site_type) {
  db_.setSiteMapEntryCbk(x, y, site_type);
}
void Driver::initClockRegionsCbk(unsigned w, unsigned h) {
  db_.initClockRegionsCbk(w, h);
}
void Driver::addNodeCbk(std::string const &node_name, std::string const &cell_name) {
  db_.addNodeCbk(node_name, cell_name);
}
void Driver::setFixedNodeCbk(std::string const &node_name, unsigned x, unsigned y, unsigned z) {
  db_.setFixedNodeCbk(node_name, x, y, z);
}
void Driver::addNetCbk(std::string const &net_name, unsigned degree) {
  db_.addNetCbk(net_name, degree);
}
void Driver::addPinCbk(std::string const &node_name, std::string const &cell_pin_name) {
  db_.addPinCbk(node_name, cell_pin_name);
}
void Driver::addClockRegionCbk(std::string const &name,
        unsigned                                  xlo,
        unsigned                                  ylo,
        unsigned                                  xhi,
        unsigned                                  yhi,
        unsigned                                  ymid,
        unsigned                                  hcxmin) {
  db_.addClockRegionCbk(name, xlo, ylo, xhi, yhi, ymid, hcxmin);
}

/* parsing cascade shape file */
void Driver::beginCascadeShapeCbk(const std::string &shape_name, int32_t width, int32_t height) {
  shape_name_ = shape_name;
  shape_size_ = std::make_pair(width, height);
  shape_list_.clear();
}
void Driver::beginCascadeShapeLineCbk() {
  shape_list_.emplace_back();
}
void Driver::addCascadeShapeLineEntryCbk(const std::string &str) {
  shape_list_.back().push_back(str);
}
void Driver::endCascadeShapeLineCbk() {
  /* do nothing */
}
void Driver::endCascadeShapeCbk() {
  db_.addCascadeShapeCbk(shape_name_, shape_size_, shape_list_);
}

/* parsing cascade shape instance file */
void Driver::beginCascadeShapeInstanceCbk(const std::string &shape_name,
        int32_t                                              width,
        int32_t                                              height,
        const std::string                                   &shape_instance_name) {
  shape_name_          = shape_name;
  shape_instance_name_ = shape_instance_name;
  shape_instance_list_.clear();
}
void Driver::addCascadeShapeInstanceEntryCbk(const std::string &str) {
  shape_instance_list_.push_back(str);
}
void Driver::endCascadeShapeInstanceCbk() {
  db_.addCascadeShapeInstanceCbk(shape_name_, shape_instance_name_, shape_instance_list_);
}

/* parsing region file */
void Driver::beginRegionConstraintCbk(int32_t region_name, int32_t num_boxes) {
  region_name_ = std::to_string(region_name);
  num_boxes_   = num_boxes;
  region_boxes_.clear();
}
void Driver::addRegionConstraintEntryCbk(const std::string &box_name,
        int32_t                                             xlo,
        int32_t                                             ylo,
        int32_t                                             xhi,
        int32_t                                             yhi) {
  region_boxes_.emplace_back((EHBookshelfDatabase::Box){box_name, xlo, ylo, xhi, yhi});
}
void Driver::endRegionConstraintCbk() {
  db_.addRegionConstraintCbk(region_name_, num_boxes_, region_boxes_);
}
void Driver::beginRegionMappingCbk() {
  /* do nothing */
}
void Driver::addRegionMappingEntryCbk(const std::string &cell_name, int32_t region_name) {
  db_.addRegionMappingCbk(cell_name, std::to_string(region_name));
}
void Driver::endRegionMappingCbk() {
  /* do nothing */
}

void Driver::addMacroInstCbk(std::string const &node_name) {
  db_.addMacroInstCbk(node_name);
}
}   // namespace ehbookshelfparser
