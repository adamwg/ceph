// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef CEPH_LIBRBD_OPERATION_QOS_REQUEST_H
#define CEPH_LIBRBD_OPERATION_QOS_REQUEST_H

#include "librbd/operation/Request.h"
#include <iosfwd>
#include <string>

class Context;

namespace librbd {

class ImageCtx;

namespace operation {

template <typename ImageCtxT = ImageCtx>
class QosRequest : public Request<ImageCtxT> {
public:
  QosRequest(ImageCtxT &image_ctx, Context *on_finish,
	     rbd_image_qos_type_t qos_type,
	     rbd_image_qos_key_t qos_key,
	     uint64_t qos_val);

protected:
  virtual void send_op();
  virtual bool should_complete(int r);

  virtual journal::Event create_event(uint64_t op_tid) const {
    return journal::QosSetEvent(op_tid, m_qos_type, m_qos_key, m_qos_val);
  }

private:
  rbd_image_qos_type_t m_qos_type;
  rbd_image_qos_key_t m_qos_key;
  uint64_t m_qos_val;

  void send_qos_request();
  Context *handle_qos_request(int *result);

  void send_notify_update();
  Context *handle_notify_update(int *result);

  Context *handle_finish(int r);
};

} // namespace operation
} // namespace librbd

extern template class librbd::operation::QosRequest<librbd::ImageCtx>;

#endif // CEPH_LIBRBD_OPERATION_QOS_REQUEST_H
