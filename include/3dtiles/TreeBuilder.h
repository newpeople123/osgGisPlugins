#ifndef OSG_GIS_PLUGINS_TREEBUILDER
#define OSG_GIS_PLUGINS_TREEBUILDER 1
#include <queue>
#include <osg/Object>
#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>
#include <osgUtil/Simplifier>
#include <osg/ComputeBoundsVisitor>
#include <iostream>
class TreeNode:public osg::Node
{
public:
	osg::ref_ptr<osg::Group> nodes;//all descendant nodes
	osg::ref_ptr<osg::Group> children;//direct child node 
	osg::ref_ptr<osg::Group> currentNodes;//current node contains nodes
	int level;//current node's level
	TreeNode* parentTreeNode;
	int x, y, z;
	double geometricError;
	std::vector<double> box;
	TreeNode() {
		nodes = new osg::Group;
		children = new osg::Group;
		currentNodes = new osg::Group;
		parentTreeNode = NULL;
	}
};

class TriangleNumberNodeVisitor:public osg::NodeVisitor
{
public:
	unsigned int count = 0;
	TriangleNumberNodeVisitor() :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {};

	void apply(osg::Drawable& drawable) {
		if (drawable.asGeometry())
		{
			osg::ref_ptr<osg::Geometry> geom = drawable.asGeometry();
			for (unsigned i = 0; i < geom->getNumPrimitiveSets(); ++i) {
				osg::ref_ptr<osg::DrawArrays> drawArrays = dynamic_cast<osg::DrawArrays*>(geom->getPrimitiveSet(i));
				if (drawArrays) {
					GLenum mode = drawArrays->getMode();
					unsigned int triangleNumber = drawArrays->getCount();
					switch (mode)
					{
					case GL_TRIANGLES:
						count += triangleNumber / 3;
						break;
					case GL_TRIANGLE_STRIP:
						count += (triangleNumber - 2);
						break;
					case GL_TRIANGLE_FAN:
						count += (triangleNumber - 2);
						break;
					case GL_QUADS:
						count += (triangleNumber / 4) * 2;
						break;
					case GL_QUAD_STRIP:
						count += triangleNumber;
						break;
					case GL_POLYGON:
						count += triangleNumber - 2;
						break;
					default:
						break;
					}
				}

				osg::ref_ptr<osg::DrawElements> drawElements = dynamic_cast<osg::DrawElements*>(geom->getPrimitiveSet(i));
				if (drawElements) {
					count += drawElements->getNumIndices() / 3;
				}
			}
		}
	}
private:

};
class RebuildDataNodeVisitor :public osg::NodeVisitor {
public:
	RebuildDataNodeVisitor() :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {};
	osg::ref_ptr<osg::Group> output = new osg::Group;
	void apply(osg::Drawable& drawable) {
		osg::ref_ptr<osg::Geode> geode = new osg::Geode;
		geode->addChild(&drawable);
		osg::MatrixList matrixList = drawable.getWorldMatrices();
		osg::Matrixd mat;
		for (const osg::Matrixd& matrix : matrixList) {
			mat = mat * matrix;
		}
		if (mat != osg::Matrixd()) {
			osg::MatrixTransform* transform = new osg::MatrixTransform;
			transform->setMatrix(mat);
			transform->addChild(geode);
			output->addChild(transform);
		}
		else {
			output->addChild(geode);
		}
	};
	void apply(osg::Geode& geode) {
		osg::Geode* copyGeode = new osg::Geode;
		std::string name = typeid(geode).name();
		for (unsigned int i = 0; i < geode.getNumChildren(); ++i) {
			osg::Node* node = geode.getChild(i);
			if (dynamic_cast<osg::Drawable*>(node) != NULL) {
				copyGeode->addChild(node);
			}
			else {
				traverse(*node);
			}
		}
		if (copyGeode->getNumChildren()) {
			osg::MatrixList matrixList = geode.getWorldMatrices();
			osg::Matrixd mat;
			for (const osg::Matrixd& matrix : matrixList) {
				mat = mat * matrix;
			}
			if (mat != osg::Matrixd()) {
				osg::MatrixTransform* transform = new osg::MatrixTransform;
				transform->setMatrix(mat);
				transform->addChild(copyGeode);
				output->addChild(transform);
			}
			else {
				output->addChild(copyGeode);
			}
		}
	};
	void apply(osg::Transform& mtransform) {
		for (unsigned int i = 0; i < mtransform.getNumChildren(); ++i) {
			osg::Node* child = mtransform.getChild(i);
			traverse(*child);
		}
	};
	void apply(osg::Group& group) {
		for (unsigned int i = 0; i < group.getNumChildren(); ++i) {
			osg::Node* child = group.getChild(i);
			if (dynamic_cast<osg::Transform*>(child)) {
				apply(static_cast<osg::Transform&>(*child));
			}
			else {
				traverse(*child);
			}
		}
	};
};
class TreeBuilder:public osg::Object
{
public:
	osg::ref_ptr<TreeNode> rootTreeNode = new TreeNode;
	TreeBuilder(const TreeBuilder& node, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY) {};
	TreeBuilder() :_maxTriangleNumber(40000), _maxTreeDepth(8), _maxLevel(0), _simpleRatio(0.5f) {
	};
	TreeBuilder(const unsigned int maxTriangleNumber, const int maxTreeDepth, const double simpleRatio) :_maxTriangleNumber(maxTriangleNumber), _maxTreeDepth(maxTreeDepth), _maxLevel(0), _simpleRatio(simpleRatio) {
	};
	TreeBuilder(osg::ref_ptr<osg::Group> node) :_maxTriangleNumber(40000), _maxTreeDepth(8), _maxLevel(0), _simpleRatio(0.5f) {
	};
	TreeBuilder(osg::ref_ptr<osg::Group> node, const unsigned int maxTriangleNumber, const int maxTreeDepth, const double simpleRatio) :_maxTriangleNumber(maxTriangleNumber), _maxTreeDepth(maxTreeDepth), _maxLevel(0), _simpleRatio(simpleRatio) {
	};
	~TreeBuilder() {}
	virtual Object* cloneType() const { return new TreeBuilder(); }
	virtual Object* clone(const osg::CopyOp& copyop) const { return new TreeBuilder(*this, copyop); }
	virtual const char* libraryName() const { return "osgGisPluginsTools"; }
	virtual const char* className() const { return "model23dtiles"; }
protected:
	//todo:create quadtree or octree
	//virtual osg::ref_ptr<TreeNode> buildTree(const osg::BoundingBox& total, const osg::ref_ptr<osg::Group>& inputRoot, int parentX = 0, int parentY = 0, int parentZ = 0, osg::ref_ptr<TreeNode> parent = nullptr, int depth = 0);
	void convertTreeNode2Levels(osg::ref_ptr<TreeNode> rootTreeNode, std::vector<std::vector<osg::ref_ptr<TreeNode>>>& levels) {
		std::queue<osg::ref_ptr<TreeNode>> q;
		q.push(rootTreeNode);
		while (!q.empty()) {
			int size = q.size();
			std::vector<osg::ref_ptr<TreeNode>> level;
			for (int i = 0; i < size; i++) {
				osg::ref_ptr<TreeNode> node = q.front();
				q.pop();
				level.push_back(node);

				for (unsigned int j = 0; j < node->children->getNumChildren();++j) {
					osg::ref_ptr<TreeNode> child = dynamic_cast<TreeNode*>(node->children->getChild(j));
					q.push(child);
				}
			}
			levels.push_back(level);
		}
	}
	void buildHlod(osg::ref_ptr<TreeNode> rootTreeNode) {
		std::vector<std::vector<osg::ref_ptr<TreeNode>>> levels;
		convertTreeNode2Levels(rootTreeNode, levels);

		for (int i = levels.size() - 1; i > -1; --i) {
			std::vector<osg::ref_ptr<TreeNode>> level = levels.at(i);
			for (osg::ref_ptr<TreeNode> treeNode : level) {
				if (treeNode->currentNodes->getNumChildren()==0) {
					for (unsigned int j = 0; j < treeNode->children->getNumChildren(); ++j) {
						osg::ref_ptr<TreeNode> childNode = dynamic_cast<TreeNode*>(treeNode->children->getChild(j));
						const osg::BoundingSphere& childBoudingSphere = childNode->currentNodes->getBound();
						for (unsigned int l = 0; l < childNode->currentNodes->getNumChildren(); ++l) {
							osg::ref_ptr<osg::Node> childNodeCurrentNode = childNode->currentNodes->getChild(l);
							if (childNodeCurrentNode.valid()) {
								if (childNodeCurrentNode->getBound().radius() * 10 >= childBoudingSphere.radius()) {
									osg::ref_ptr<osg::Node> node = osg::clone(childNodeCurrentNode.get(), osg::CopyOp::DEEP_COPY_ALL);
									treeNode->currentNodes->addChild(node);
								}
							}
						}
						if (treeNode->currentNodes->getNumChildren()) {
							osg::ref_ptr<osg::Node> lodModel = treeNode->currentNodes.get();
							osgUtil::Simplifier simplifier;
							simplifier.setSampleRatio(_simpleRatio);
							lodModel->accept(simplifier);
						}
					}
				}
			}
		}
		computeBoundingVolume(rootTreeNode);

		//hlodOptimizierProxy(rootTreeNode);
	}
	osg::BoundingBox getBoundingBox(osg::ref_ptr<osg::Node> node) {
		osg::ComputeBoundsVisitor cbbv;
		node->accept(cbbv);
		return cbbv.getBoundingBox();
	}

protected:
	unsigned int _maxTriangleNumber;
	int _maxTreeDepth;
	int _maxLevel;
	double _simpleRatio;
private:
	osg::ref_ptr<osg::Group> hlodOptimizier(osg::ref_ptr<osg::Group> nodes) {
		std::map<osg::Matrixd, osg::ref_ptr<osg::MatrixTransform>> transformMap;
		osg::Matrixd identityMatrix;
		for (unsigned i = 0; i < rootTreeNode->nodes->getNumChildren(); ++i) {
			//node is MatrixTransform or Geode
			osg::ref_ptr<osg::Node> node = rootTreeNode->nodes->getChild(i);
			osg::ref_ptr<osg::MatrixTransform> transform = dynamic_cast<osg::MatrixTransform*>(node.get());
			if (transform.valid()) {
				osg::Matrixd key = transform->getMatrix();
				std::map<osg::Matrixd, osg::ref_ptr<osg::MatrixTransform>>::iterator findResult = transformMap.find(key);
				if (findResult != transformMap.end()) {
					for (unsigned int j = 0; j < transform->getNumChildren(); ++j) {
						findResult->second->addChild(transform->getChild(j));
					}
				}
				else {
					osg::ref_ptr<osg::MatrixTransform> val = new osg::MatrixTransform;
					for (unsigned int j = 0; j < transform->getNumChildren(); ++j) {
						val->addChild(transform->getChild(j));
					}
					transformMap.insert(std::make_pair(key, val));
				}
			}

			else {
				std::map<osg::Matrixd, osg::ref_ptr<osg::MatrixTransform>>::iterator findResult = transformMap.find(identityMatrix);
				if (findResult != transformMap.end()) {
					findResult->second->addChild(node);
				}
				else {
					osg::ref_ptr<osg::MatrixTransform> val = new osg::MatrixTransform;
					val->addChild(node);
					transformMap.insert(std::make_pair(identityMatrix, val));
				}
			}
		}

		osg::ref_ptr<osg::Group> result = new osg::Group;
		for (auto it = transformMap.begin(); it != transformMap.end(); ++it) {
			result->addChild(it->second);
		}
		return result;
	}
	void hlodOptimizierProxy(osg::ref_ptr<TreeNode> rootTreeNode) {
		for (unsigned int i = 0; i < rootTreeNode->children->getNumChildren(); ++i) {
			hlodOptimizierProxy(dynamic_cast<TreeNode*>(rootTreeNode->children->getChild(i)));
		}
		rootTreeNode->nodes = hlodOptimizier(rootTreeNode->nodes);
		rootTreeNode->currentNodes = hlodOptimizier(rootTreeNode->currentNodes);

	}
	void computeBoundingVolume(osg::ref_ptr<TreeNode> treeNode) {
		if (treeNode->currentNodes->getNumChildren()) {
			osg::ComputeBoundsVisitor computeBoundsVisitor;
			treeNode->currentNodes->accept(computeBoundsVisitor);
			osg::BoundingBox boundingBox = computeBoundsVisitor.getBoundingBox();
			const osg::Vec3f size = boundingBox._max - boundingBox._min;
			const osg::Vec3f cesiumBoxCenter = boundingBox.center();

			treeNode->box.push_back(cesiumBoxCenter.x());
			treeNode->box.push_back(cesiumBoxCenter.y());
			treeNode->box.push_back(cesiumBoxCenter.z());

			treeNode->box.push_back(size.x() / 2);
			treeNode->box.push_back(0);
			treeNode->box.push_back(0);

			treeNode->box.push_back(0);
			treeNode->box.push_back(size.y() / 2);
			treeNode->box.push_back(0);

			treeNode->box.push_back(0);
			treeNode->box.push_back(0);
			treeNode->box.push_back(size.z() / 2);
		}
		for (unsigned int i = 0; i < treeNode->children->getNumChildren(); ++i) {
			computeBoundingVolume(dynamic_cast<TreeNode*>(treeNode->children->getChild(i)));
		}
	}
};
#endif // !OSG_GIS_PLUGINS_TREEBUILDER
