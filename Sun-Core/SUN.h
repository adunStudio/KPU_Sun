#pragma once
//
// Created by adunstudio on 2018-01-06.
//

#pragma once

// �ʿ��� ���� ���̺귯������ Include �Ѵ�.
#include <cstddef>
#include <cstdio>
#include <cmath>
#include <cstring>

/* STL */
#include <algorithm>
#include <functional>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

/* STL Container */
#include <vector>
#include <list>
#include <map>
#include <unordered_map>


// ��Ʈ�� ��ȯ�Ѵ�.
#define BIT(x) (1 << x)

// �Լ��� ���ε��Ѵ�.
#define METHOD_1(x) std::bind(x, this, std::placeholders::_1)
#define METHOD(x)   METHOD_1(x)
