#include "3dtiles/hlod/QuadtreeBuilder.h"
osg::ref_ptr<Tile> QuadtreeBuilder::divide(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<Tile> parent, const int level)
{
	osg::ref_ptr<Tile> tile = new Tile(parent);
	tile->level = level;

	const unsigned int numChildren = group->getNumChildren();
	if (numChildren <= 2)
	{
		tile->parent = parent;
		tile->node = group;
		return tile;
	}

	TextureCountVisitor textureCv;
	group->accept(textureCv);

	TriangleCountVisitor tcv;
	group->accept(tcv);

	// 如果纹理数量和三角面数量满足要求
	if (textureCv.count <= _maxTextureCount && tcv.count <= _maxTriangleCount)
	{
		tile->parent = parent;
		tile->node = group;
		return tile;
	}

	// 如果当前树的深度超过最大深度或者当前节点为空节点
	if (level >= _maxLevel || numChildren == 0)
	{
		tile->parent = parent;
		tile->node = group;
		return tile;
	}


	// Calculate midpoint  
	osg::Vec3f mid = (bounds._max + bounds._min) * 0.5f;

	// Divide space into 8 parts (octree)  
	for (int x = 0; x < 2; ++x)
	{
		for (int y = 0; y < 2; ++y)
		{
			osg::BoundingBox childBounds;
			childBounds._min.set(
				x ? mid.x() : bounds._min.x(),
				y ? mid.y() : bounds._min.y(),
				bounds._min.z()
			);
			childBounds._max.set(
				x ? bounds._max.x() : mid.x(),
				y ? bounds._max.y() : mid.y(),
				bounds._max.z()
			);

			osg::ref_ptr<osg::Group> childGroup = new osg::Group;
			for (unsigned int i = 0; i < numChildren; ++i)
			{
				osg::ref_ptr<osg::Node> child = group->getChild(i);
				if (childBounds.contains(child->getBound().center()))
				{
					childGroup->addChild(child);
				}
			}
			if (childGroup->getNumChildren() > 0)
			{
				tile->children.push_back(divide(childGroup, childBounds, tile, level + 1));
				tile->children.back()->x = x;
				tile->children.back()->y = y;

			}
		}
	}

	return tile;
}
