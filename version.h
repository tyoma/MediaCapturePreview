#pragma once
#define MCP_VERSION_MAJOR 1
#define MCP_VERSION_MINOR 0
#define MCP_BUILD 1
#define MCP_BRANCH 0
#define MAKE_TOKEN(token) #token
#define MAKE_VERSION_STRING(major, minor, build, branch) MAKE_TOKEN(major) "." MAKE_TOKEN(minor) "." MAKE_TOKEN(build) "." MAKE_TOKEN(branch)
#define MCP_VERSION_STR MAKE_VERSION_STRING(MCP_VERSION_MAJOR, MCP_VERSION_MINOR, MCP_BUILD, MCP_BRANCH)
#define MCP_VERSION_RES MCP_VERSION_MAJOR,MCP_VERSION_MINOR,MCP_BUILD,MCP_BRANCH
#define MCP_PRODUCT_NAME ""
#define MCP_PRODUCT_COMPANY_NAME ""
#define MCP_PRODUCT_COPYRIGHT ""
