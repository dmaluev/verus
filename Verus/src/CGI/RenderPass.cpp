#include "verus.h"

using namespace verus;
using namespace verus::CGI;
using namespace verus::CGI::RP;

// Attachment:

Attachment::Attachment(CSZ name, Format format, int sampleCount) :
	_name(name),
	_format(format),
	_sampleCount(sampleCount)
{
}

RAttachment Attachment::LoadOpClear()
{
	_loadOp = LoadOp::clear;
	return *this;
}

RAttachment Attachment::LoadOpDontCare() {
	_loadOp = LoadOp::dontCare;
	return *this;
}

RAttachment Attachment::StoreOpDontCare()
{
	_storeOp = StoreOp::dontCare;
	return *this;
}

RAttachment Attachment::Layout(ImageLayout whenBegins, ImageLayout whenEnds)
{
	_initialLayout = whenBegins;
	_finalLayout = whenEnds;
	return *this;
}

RAttachment Attachment::Layout(ImageLayout both)
{
	_initialLayout = both;
	_finalLayout = both;
	return *this;
}

RAttachment Attachment::FinalLayout(ImageLayout whenEnds)
{
	_finalLayout = whenEnds;
	return *this;
}

// Subpass:

Subpass::Subpass(CSZ name) :
	_name(name)
{
}

RSubpass Subpass::Input(std::initializer_list<Ref> il)
{
	_ilInput = il;
	return *this;
}

RSubpass Subpass::Color(std::initializer_list<Ref> il)
{
	_ilColor = il;
	return *this;
}

RSubpass Subpass::Resolve(std::initializer_list<Ref> il)
{
	_ilResolve = il;
	return *this;
}

RSubpass Subpass::Preserve(std::initializer_list<Ref> il)
{
	_ilPreserve = il;
	return *this;
}

RSubpass Subpass::DepthStencil(Ref r)
{
	_depthStencil = r;
	return *this;
}

// Dependency:

Dependency::Dependency()
{
}

RDependency Dependency::Src(CSZ name)
{
	return *this;
}

RDependency Dependency::Dst(CSZ name)
{
	return *this;
}
