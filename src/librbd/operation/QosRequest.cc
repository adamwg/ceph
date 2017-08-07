// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "librbd/operation/QosRequest.h"
#include "common/dout.h"
#include "common/errno.h"
#include "librbd/ImageCtx.h"
#include "librbd/ImageState.h"
#include "librbd/io/ImageRequestWQ.h"
#include "librbd/internal.h"
#include "librbd/ExclusiveLock.h"
#include "librbd/Journal.h"
#include "librbd/Utils.h"

#define dout_subsys ceph_subsys_rbd
#undef dout_prefix
#define dout_prefix *_dout << "librbd::QosRequest: "

namespace librbd {
namespace operation {

using util::create_async_context_callback;
using util::create_context_callback;
using util::create_rados_callback;

template <typename I>
QosRequest<I>::QosRequest(I &image_ctx, Context *on_finish,
			  rbd_image_qos_type_t qos_type,
			  rbd_image_qos_key_t qos_key,
			  uint64_t qos_val)
  : Request<I>(image_ctx, on_finish), m_qos_type(qos_type),
    m_qos_key(qos_key), m_qos_val(qos_val) {
}

template <typename I>
void QosRequest<I>::send_op() {
  I &image_ctx = this->m_image_ctx;
  CephContext *cct = image_ctx.cct;
  assert(image_ctx.owner_lock.is_locked());

  ldout(cct, 20) << this << " " << __func__ << dendl;
  send_qos_request();
}

template <typename I>
bool QosRequest<I>::should_complete(int r) {
  I &image_ctx = this->m_image_ctx;
  CephContext *cct = image_ctx.cct;
  ldout(cct, 20) << this << " " << __func__ << "r=" << r << dendl;

  if (r < 0) {
    lderr(cct) << "encountered error: " << cpp_strerror(r) << dendl;
  }
  return true;
}

template <typename I>
void QosRequest<I>::send_qos_request() {
  I &image_ctx = this->m_image_ctx;
  assert(image_ctx.owner_lock.is_locked());

  CephContext *cct = image_ctx.cct;
  ldout(cct, 5) << this << " " << __func__ << dendl;

  {
    RWLock::RLocker md_locker(image_ctx.md_lock);

    std::map<std::string, bufferlist> metadata;
    metadata[get_qos_metadata_key(m_qos_type, m_qos_key)].append(std::to_string(m_qos_val));
    librados::ObjectWriteOperation op;
    cls_client::metadata_set(&op, metadata);

    using klass = QosRequest<I>;
    librados::AioCompletion *comp = create_rados_callback<klass, &klass::handle_qos_request>(this);
    int r = image_ctx.md_ctx.aio_operate(image_ctx.header_oid, comp, &op);
    assert(r == 0);
    comp->release();
  }
}

template <typename I>
Context *QosRequest<I>::handle_qos_request(int *result) {
  I &image_ctx = this->m_image_ctx;
  CephContext *cct = image_ctx.cct;
  ldout(cct, 20) << this << " " << __func__ << ": r=" << *result << dendl;

  if (*result < 0) {
    lderr(cct) << "failed to set qos: "
               << cpp_strerror(*result) << dendl;
    return handle_finish(*result);
  }

  send_notify_update();
  return nullptr;
}

template <typename I>
void QosRequest<I>::send_notify_update() {
  I &image_ctx = this->m_image_ctx;
  CephContext *cct = image_ctx.cct;
  ldout(cct, 20) << this << " " << __func__ << dendl;

  Context *ctx = create_context_callback<
    QosRequest<I>,
    &QosRequest<I>::handle_notify_update>(this);

  image_ctx.notify_update(ctx);
}

template <typename I>
Context *QosRequest<I>::handle_notify_update(int *result) {
  I &image_ctx = this->m_image_ctx;
  CephContext *cct = image_ctx.cct;
  ldout(cct, 20) << this << " " << __func__ << ": r=" << *result << dendl;

  return handle_finish(*result);
}

template <typename I>
Context *QosRequest<I>::handle_finish(int r) {
  I &image_ctx = this->m_image_ctx;
  CephContext *cct = image_ctx.cct;
  ldout(cct, 20) << this << " " << __func__ << ": r=" << r << dendl;

  return this->create_context_finisher(r);
}

} // namespace operation
} // namespace librbd

template class librbd::operation::QosRequest<librbd::ImageCtx>;
