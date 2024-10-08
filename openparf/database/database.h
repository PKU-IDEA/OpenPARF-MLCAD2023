/*
 * @file          : database.h
 * @project       : database
 * @author        : Jing Mai <jingmai@pku.edu.cn>
 * @created date  : June 28 2023, 15:51:51, Wednesday
 * @brief         :
 * -----
 * Last Modified: July 14 2023, 15:45:56, Friday
 * Modified By: Jing Mai <jingmai@pku.edu.cn>
 * -----
 * @history :
 * ====================================================================================
 * Date         	By     	(version)	Comments
 * -------------	-------	---------	--------------------------------------------------
 * ====================================================================================
 * Copyright (c) 2020 - 2023 All Right Reserved, PKU-IDEA Group
 * -----
 * This header is generated by VSCode extension psi-header.
 */
#ifndef OPENPARF_DATABASE_DATABASE_H_
#define OPENPARF_DATABASE_DATABASE_H_

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "database/design.h"
#include "database/layout.h"
#include "database/region.h"
#include "database/shape.h"

OPENPARF_BEGIN_NAMESPACE

namespace database {

/// @brief Entire database including hierarchy
/// and all information.
class Database : public Object {
 public:
  using BaseType = Object;

  /// @brief Assumption of parameters for the database
  struct Params : public Object {
    IndexType            arch_half_column_region_width_ = 2;   ///< width of a half column

    /// @brief overload output stream
    friend std::ostream &operator<<(std::ostream &os, Params const &rhs);
  };

  /// @brief default constructor
  Database() : BaseType(), design_(0), layout_(0) {}

  /// @brief constructor
  explicit Database(IndexType id) : BaseType(id), design_(0), layout_(0) {}

  /// @brief copy constructor
  Database(Database const &rhs) { copy(rhs); }

  /// @brief move constructor
  explicit Database(Database &&rhs) { move(std::move(rhs)); }

  /// @brief copy assignment
  Database &operator=(Database const &rhs) {
    if (this != &rhs) {
      copy(rhs);
    }
    return *this;
  }

  /// @brief move assignment
  Database &operator=(Database &&rhs) noexcept {
    if (this != &rhs) {
      move(std::move(rhs));
    }
    return *this;
  }

  /// @brief getter for design
  Design const           &design() const { return design_; }

  /// @brief getter for design
  Design                 &design() { return design_; }

  /// @brief getter for layout
  Layout const           &layout() const { return layout_; }

  /// @brief getter for layout
  Layout                 &layout() { return layout_; }

  /// @brief getter for shape constraints
  ShapeConstraint const  &shapeConstraint() const { return shape_constraint_; }

  /// @brief getter for shape constraints
  ShapeConstraint        &shapeConstraint() { return shape_constraint_; }

  /// @brief getter for region constraints
  RegionConstraint const &regionConstraint() const { return region_constraint_; }

  /// @brief getter for region constraints
  RegionConstraint       &regionConstraint() { return region_constraint_; }

  /// @brief getter for parameters
  Params const           &params() const { return params_; }

  /// @brief getter for inst macro ids
  std::vector<IndexType> &instMacroIds() { return inst_macro_ids_; }

  /// @brief add inst macro ids
  void                    addInstMacroId(IndexType id) { inst_macro_ids_.push_back(id); }

  /// @brief getter for parameters
  Params                 &params() { return params_; }

  /// @brief read from XML files
  void                    readXML(std::string const &xml_file);

  /// @brief read from verilog files
  void                    readVerilog(std::string const &verilog_file);

  /// @brief read from bookshelf files
  void                    readBookshelf(std::string const &aux_file);

  /// @brief read from enhanced bookshelf files
  void                    readEHBookshelf(std::string const &input_dir);

  /// @brief write bookshelf .pl file
  bool                    writeBookshelfPl(std::string const &pl_file);

  /// @brief write bookshelf .pl file for macros, speciakized for MLCAD 2023 FPGA macro placement contest
  bool                    writeMacroBookshelfPl(std::string const &pl_file);

  /// @brief write bookshelf .nodes file
  bool                    writeBookshelfNodes(std::string const &nodes_file);

  /// @brief write bookshelf .nets file
  bool                    writeBookshelfNets(std::string const &nets_file);

  /// @brief read from flexshelf files
  void readFlexshelf(std::string const &layout_file, std::string const &netlist_file, std::string const &place_file);

  // @brief read from flexshelf layout file
  void readFlexshelfLayout(std::string const &layout_file);

  // @brief read from flexshelf design file
  void readFlexshelfDesign(std::string const &netlist_file, std::string const &place_file);

  /// @brief summarize memory usage of the object in bytes
  IndexType memory() const;

 protected:
  /// @brief copy object
  void                   copy(Database const &rhs);
  /// @brief move object
  void                   move(Database &&rhs);
  /// @brief overload output stream
  friend std::ostream   &operator<<(std::ostream &os, Database const &rhs);

  Design                 design_;              ///< design information
  Layout                 layout_;              ///< layout information
  ShapeConstraint        shape_constraint_;    ///< shape constraint
  RegionConstraint       region_constraint_;   ///< region constraint
  Params                 params_;              ///< database parameters


  /// specialized for MLCAD 2023 FPGA Macro Placement Contest
  std::vector<IndexType> inst_macro_ids_;
};

}   // namespace database

OPENPARF_END_NAMESPACE

#endif   // OPENPARF_DATABASE_DATABASE_H_
