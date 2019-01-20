/* Copyright (C) Teemu Suutari */

#include "SubBuffer.hpp"

template <>
uint8_t *GenericSubBuffer<Buffer>::data()
{
	return _base.data()+_start;
}

template <>
uint8_t *GenericSubBuffer<const Buffer>::data()
{
	throw InvalidOperationError();
}
