// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef GGL_IPC_AUTH_H
#define GGL_IPC_AUTH_H

//! Greengrass IPC authentication interface
//!
//! This module implements an interface for a GG-IPC server to validate received
//! SVCUID tokens, and a means for components to obtain SVCUID tokens.

#include <ggl/error.h>
#include <ggl/object.h>

/// Gets the component name associated with an SVCUID.
GglError ggl_ipc_auth_get_component_name(
    GglBuffer svcuid, GglBuffer *component_name
);

#endif