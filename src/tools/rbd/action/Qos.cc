// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "tools/rbd/ArgumentTypes.h"
#include "tools/rbd/Shell.h"
#include "tools/rbd/Utils.h"
#include "include/types.h"
#include "include/stringify.h"
#include "common/errno.h"
#include "common/Formatter.h"
#include "common/Cond.h"
#include "common/TextTable.h"
#include <iostream>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/rolling_sum.hpp>
#include <boost/program_options.hpp>

namespace rbd {
namespace action {
namespace qos {

namespace at = argument_types;
namespace po = boost::program_options;

namespace {

struct Size {};

void validate(boost::any& v, const std::vector<std::string>& values,
              Size *target_type, int) {
  po::validators::check_first_occurrence(v);
  const std::string &s = po::validators::get_single_string(values);

  std::string parse_error;
  uint64_t size = strict_sistrtoll(s.c_str(), &parse_error);
  if (!parse_error.empty()) {
    throw po::validation_error(po::validation_error::invalid_option_value);
  }
  v = boost::any(size);
}
} // anonymous namespace

int do_qos_set(librbd::Image& image, rbd_image_qos_type_t qos_type,
	       rbd_image_qos_key_t qos_key, uint64_t qos_val)
{
  return image.qos_set(qos_type, qos_key, qos_val);
}

void get_iops_arguments(po::options_description *pos,
			po::options_description *opt) {
  at::add_image_spec_options(pos, opt, at::ARGUMENT_MODIFIER_NONE);
  opt->add_options()
    ("limit", po::value<Size>(), "average of iops we allow(in B/K/M/G/T).")
    ("io-type", po::value<std::string>(), "type of iops we want to limit.");
}

int execute_iops(const po::variables_map &vm) {
  size_t arg_index = 0;
  std::string pool_name;
  std::string image_name;
  std::string snap_name;
  uint64_t limit;
  std::string type_str;
  rbd_image_qos_type_t qos_type;

  int r = utils::get_pool_image_snapshot_names(
    vm, at::ARGUMENT_MODIFIER_NONE, &arg_index, &pool_name, &image_name,
    &snap_name, utils::SNAPSHOT_PRESENCE_NONE, utils::SPEC_VALIDATION_NONE);

  if (vm.count("limit")) {
    limit = vm["limit"].as<uint64_t>();
  } else {
    std::cerr << "Please specify the limit we allow" << std::endl;
    return -EINVAL;
  }

  if (vm.count("io-type")){
    type_str = vm["io-type"].as<std::string>();
  }else{
    std::cerr << "Please specify the io-type we want to limit" << std::endl;
    return -EINVAL;
  }

  librados::Rados rados;
  librados::IoCtx io_ctx;
  librbd::Image image;
  r = utils::init_and_open_image(pool_name, image_name, "", "", false,
                                 &rados, &io_ctx, &image);
  if (r < 0) {
      return r;
  }

  if (type_str == "read") {
    qos_type = RBD_IMAGE_QOS_TYPE_IOPS_READ;
  } else if (type_str == "write") {
    qos_type = RBD_IMAGE_QOS_TYPE_IOPS_WRITE;
  } else {
    std::cerr << "Not-supported iops qos type specified: " << type_str << std::endl;
    return -EINVAL;
  }

  r = do_qos_set(image, qos_type, RBD_IMAGE_QOS_KEY_AVG, limit);

  if (r < 0) {
    std::cerr << "rbd: setting iops limit failed: " << cpp_strerror(r)
	      << std::endl;
    return r;
  }
  return 0;
}


Shell::Action action_list(
  {"qos", "iops"}, {}, "Set the iops limit on RBD.", "",
  &get_iops_arguments, &execute_iops);
  // TODO add bps subcommand for bandwidth per second limiting.
} // namespace qos
} // namespace action
} // namespace rbd
