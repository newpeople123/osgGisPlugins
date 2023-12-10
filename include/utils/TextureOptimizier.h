#ifndef OSG_GIS_PLUGINS_TEXTURE_OPTIMIZIER_H
#define OSG_GIS_PLUGINS_TEXTURE_OPTIMIZIER_H 1
#include <osg/NodeVisitor>
#include <osg/Texture>
#include <utils/TextureAtlas.h>
#include <future>
#include <osgViewer/Viewer>
struct ImageSortKey
{
	GLenum pixelFormat;
	unsigned int packing;
	bool operator<(const ImageSortKey& other) const {
		if (pixelFormat < other.pixelFormat) {
			return true;
		}
		else if (pixelFormat > other.pixelFormat) {
			return false;
		}

		return packing < other.packing;
	}
};
typedef std::map<ImageSortKey, std::vector<osg::ref_ptr<osg::Image>>> ImageSortMap;
class TextureVisitor :public osg::NodeVisitor
{
public:
	TextureVisitor() :osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {

	}
	void apply(osg::Node& node) override
	{
		osg::StateSet* ss = node.getStateSet();
		const osg::ref_ptr<osg::Geometry> geom = node.asGeometry();
		if (geom.valid()) {
			const osg::ref_ptr<osg::Vec2Array> texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));
			if (texCoords.valid()) {
				for (auto coord : *texCoords)
				{
					const double u = coord.x();
					const double v = coord.y();
					if (u > 1.0 || u < -1.0 || v>1.0 || v < -1.0) {
						return;
					}
				}
			}
		}
		if (ss) {
			apply(*ss);
		}
		traverse(node);
	}
	void apply(osg::StateSet& stateset) {
		for (unsigned int i = 0; i < stateset.getTextureAttributeList().size(); ++i)
		{
			osg::StateAttribute* sa = stateset.getTextureAttribute(i, osg::StateAttribute::TEXTURE);
			osg::Texture* texture = dynamic_cast<osg::Texture*>(sa);
			if (texture)
			{
				apply(*texture);
			}
		}
	}
	void apply(osg::Texture& texture) {
		for (unsigned int i = 0; i < texture.getNumImages(); i++) {
			osg::ref_ptr<osg::Image> img = texture.getImage(i);
			bool isAddToImgs = true;
			for (const auto& j : imgs)
			{
				if (euqalImage(img, j)) {
					isAddToImgs = false;
					break;
				}
			}
			if (isAddToImgs)
			{
				//img->flipVertical();
				imgs.push_back(img);
			}
		}
	}
	bool euqalImage(const osg::ref_ptr<osg::Image>& img1,const osg::ref_ptr<osg::Image>& img2) {
		const std::string filename1 = osgDB::convertStringFromUTF8toCurrentCodePage(img1->getFileName());
		const std::string filename2 = osgDB::convertStringFromUTF8toCurrentCodePage(img2->getFileName());

		if (!filename1.empty() || !filename2.empty())
		{
			if (filename1 == filename2)
				return true;
			return false;
		}
		else if (filename1 == filename2 && filename1.empty()) {
			if (img1->s() != img2->s() || img1->r() != img2->r() || img1->t() != img2->t()) {
				return false;
			}
			if (img1->getPixelFormat() != img2->getPixelFormat()) {
				return false;
			}
			if (img1->getPacking() != img2->getPacking()) {
				return false;
			}
			if (img1->getTotalDataSize() != img2->getTotalDataSize()) {
				return false;
			}
			const unsigned char* data1 = img1->data();
			const std::vector<unsigned char> dataVector1(data1, data1 + img1->getTotalDataSize());
			const unsigned char* data2 = img2->data();
			const std::vector<unsigned char> dataVector2(data2, data2 + img2->getTotalDataSize());

			for (unsigned int i = 0; i < dataVector1.size(); ++i) {
				if (dataVector1[i] != dataVector2[i]) {
					return false;
				}
			}
			return true;

		}
		return false;
	}
	void groupImages() {
		for (auto& img : imgs) {
			ImageSortKey key;
			key.pixelFormat = img->getPixelFormat();

			key.packing = img->getPacking();
			auto val = ism.find(key);
			if (val != ism.end()) {
				val->second.push_back(img);
			}
			else {
				std::vector<osg::ref_ptr<osg::Image>> images;
				images.push_back(img);
				ism.insert(std::make_pair(key, images));
			}
		}

		for (auto& pair : ism) {
			std::vector<osg::ref_ptr<osg::Image>>& value = pair.second;
			std::sort(value.begin(), value.end(), [](const osg::ref_ptr<osg::Image>& img1, const osg::ref_ptr<osg::Image>& img2) {
				if (img1->s() * img1->t() < img2->s() * img2->t()) {
					return true;
				}
				else if (img1->s() * img1->t() == img2->s() * img2->t()) {
					if (img1->s() < img2->s()) {
						return true;
					}
					else if (img1->s() > img2->s()) {
						return false;
					}
					else {
						return img1->t() < img2->t();
					}
				}
				else {
					return false;
				}
				});

		}
	}
	std::vector<osg::ref_ptr<osg::Image>> imgs;
	ImageSortMap ism;
	~TextureVisitor() override
	{
		std::fill(imgs.begin(), imgs.end(), nullptr);

		for (auto& item : ism) {
			std::fill(item.second.begin(), item.second.end(), nullptr);
		}
	};
private:

};

class TextureOptimizerResolve :public osg::NodeVisitor {
	std::vector<TextureAtlas*> textureAtlases;
public:
	explicit TextureOptimizerResolve(const std::vector<TextureAtlas*>& textureAtlases) :osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {
		this->textureAtlases = textureAtlases;
	}
	void apply(osg::Drawable& geom) override
	{
		osg::StateSet* ss = geom.getStateSet();
		const osg::ref_ptr<osg::Vec2Array> texCoords = dynamic_cast<osg::Vec2Array*>(geom.asGeometry()->getTexCoordArray(0));
		if (ss) {
			for (unsigned int i = 0; i < ss->getTextureAttributeList().size(); ++i)
			{
				osg::StateAttribute* sa = ss->getTextureAttribute(i, osg::StateAttribute::TEXTURE);
				osg::Texture* texture = dynamic_cast<osg::Texture*>(sa);
				if (texture)
				{
					for (unsigned int j = 0; j < texture->getNumImages(); j++) {
						osg::ref_ptr<osg::Image> img = texture->getImage(j);
						const std::string filename = osgDB::convertStringFromUTF8toCurrentCodePage(img->getFileName());

						bool isRepeat = false;
						for (auto& q : *texCoords)
						{
							const double u = q.x();
							const double v = q.y();
							if (u > 1.0 || u < -1.0 || v>1.0 || v < -1.0) {
								isRepeat = true;
								break;
							}
						}
						if(!isRepeat) {
							for (const auto textureAtlas : textureAtlases)
							{
								const auto& item = textureAtlas->imgUVRangeMap.find(filename);
								if (item != textureAtlas->imgUVRangeMap.end()) {
									const UVRange uvRangle = item->second;
									const osg::ref_ptr<osg::Image>& newImg = textureAtlas->texture();
									texture->setImage(j, newImg);


									for (auto& q : *texCoords)
									{
										osg::Vec2 coord = q;
										const double oldU = coord.x();
										const double oldV = coord.y();
										double newU = uvRangle.startU + (uvRangle.endU - uvRangle.startU) * oldU;
										double newV = uvRangle.startV + (uvRangle.endV - uvRangle.startV) * oldV;
										if (oldU < 0) {
											newU = -newU;
										}
										if (oldV < 0) {
											newV = -newV;
										}
										const osg::Vec2 newCoord(newU, newV);
										q = newCoord;
									}
									img.release();
									break;
								}
							}
						}
					}
				}
			}
		}
	}
};
class TextureOptimizer {
public:
	TextureOptimizer(const TextureOptimizer& other)
		: tv(new TextureVisitor(*(other.tv))), textureAtlases(copyTextureAtlases(other.textureAtlases)) {
	}
	TextureOptimizer(const osg::ref_ptr<osg::Node>& node, TextureType textureType,double textureMaxSize) {
		this->textureMaxSize = textureMaxSize;
		tv = new TextureVisitor;
		node->accept(*tv);
		tv->groupImages();
		buildTextureAtlas();
		TextureOptimizerResolve tos(textureAtlases);
		node->accept(tos);
		auto exportImage = [textureType](const osg::ref_ptr<osg::Image>& img) {
			//filename += "-" + std::to_string(img->s()) + "-" + std::to_string(img->t());
			if(img->getOrigin() == osg::Image::BOTTOM_LEFT)
				img->flipVertical();
			if (img->getOrigin() == osg::Image::BOTTOM_LEFT)
			{
				img->setOrigin(osg::Image::TOP_LEFT);
			}

			std::string data(reinterpret_cast<char const*>(img->data()));
			std::string filename = Stringify() << std::hex << hashString(data);
			filename += "-w" + std::to_string(img->s()) + "-h" + std::to_string(img->r());
			img->setFileName(filename);
			if (textureType == PNG) {
				std::ifstream fileExists(filename + ".png");
				if (!fileExists.good()|| (fileExists.peek() == std::ifstream::traits_type::eof())) {
					bool result = osgDB::writeImageFile(*img, filename + ".png");
					if(!result){
						std::ifstream fileExistsJpg(filename + ".jpg");
						if (!fileExistsJpg.good()|| (fileExistsJpg.peek() == std::ifstream::traits_type::eof()))
							osgDB::writeImageFile(*img, filename + ".jpg");
						fileExistsJpg.close();
					}
				}
				fileExists.close();
			}
			else if (textureType == JPG) {
				const GLenum pixelFormat = img->getPixelFormat();
				if (pixelFormat == GL_ALPHA || pixelFormat == GL_RGB) {
					std::ifstream fileExists(filename + ".jpg");
					if (!fileExists.good()|| (fileExists.peek() == std::ifstream::traits_type::eof())) {
						// Only cater for gray, alpha and RGB for now
						bool result = osgDB::writeImageFile(*img, filename + ".jpg");
						if (!result) {
							std::ifstream fileExistsPng(filename + ".png");
							if (!fileExistsPng.good() || (fileExistsPng.peek() == std::ifstream::traits_type::eof()))
								osgDB::writeImageFile(*img, filename + ".png");
							fileExistsPng.close();
						}
					}
					fileExists.close();
				}
				else {
					std::ifstream fileExistsPng(filename + ".png");
					if (!fileExistsPng.good() || (fileExistsPng.peek() == std::ifstream::traits_type::eof()))
						osgDB::writeImageFile(*img, filename + ".png");
					fileExistsPng.close();
				}
			}
			else if (textureType == WEBP) {
				std::ifstream fileExists(filename + ".webp");
				if (!fileExists.good() || (fileExists.peek() == std::ifstream::traits_type::eof())){
					img->flipVertical();
					bool result = osgDB::writeImageFile(*img, filename + ".webp");
					if (!result) {
						img->flipVertical();
						std::ifstream fileExistsPng(filename + ".png");
						if (!fileExistsPng.good() || (fileExistsPng.peek() == std::ifstream::traits_type::eof()))
							osgDB::writeImageFile(*img, filename + ".png");
						fileExistsPng.close();
					}
				}
				fileExists.close();
			}
			else if (textureType == KTX2) {
				std::ifstream fileExists(filename + ".ktx");
				if (!fileExists.good() || (fileExists.peek() == std::ifstream::traits_type::eof())){
					bool result = osgDB::writeImageFile(*img, filename + ".ktx");
					if (!result) {
						std::ifstream fileExistsPng(filename + ".png");
						if (!fileExistsPng.good() || (fileExistsPng.peek() == std::ifstream::traits_type::eof()))
							osgDB::writeImageFile(*img, filename + ".png");
						fileExistsPng.close();
					}
				}
				fileExists.close();
			}
			else if (textureType == KTX) {
				std::ifstream fileExists(filename + ".ktx");
				if (!fileExists.good() || (fileExists.peek() == std::ifstream::traits_type::eof())) {
					bool result = osgDB::writeImageFile(*img, filename + ".ktx");
					if (!result) {
						std::ifstream fileExistsPng(filename + ".png");
						if (!fileExistsPng.good() || (fileExistsPng.peek() == std::ifstream::traits_type::eof()))
							osgDB::writeImageFile(*img, filename + ".png");
						fileExistsPng.close();
					}
				}
				fileExists.close();
			}
			else {
				std::ifstream fileExists(filename + ".png");
				if (!fileExists.good() || (fileExists.peek() == std::ifstream::traits_type::eof())) {
					if (!(osgDB::writeImageFile(*img, filename + ".png"))) {
						osg::notify(osg::FATAL) << std::endl;
					}
				}
				fileExists.close();
			}
			};

		std::vector<std::future<void>> futures;
		for (const auto& textureAtlase : textureAtlases)
		{
			//if(textureAtlase->imgs.size()==1)
			//{
			//	exportImage(textureAtlase->imgs.at(0));
			//}
			//else
			//{
			//	exportImage(textureAtlase->texture());
			//}
			exportImage(textureAtlase->texture());
			//futures.push_back(std::async(std::launch::async, exportImage, textureAtlases.at(i)->texture()));
		}
		for (auto& future : futures) {
			future.get();
		}
	}
	void buildTextureAtlas() {
		for (auto& item : tv->ism) {
			while (!item.second.empty())
			{
				TextureAtlas* ta = new TextureAtlas(TextureAtlasOptions(osg::Vec2(1024, 1024), osg::Vec2(textureMaxSize, textureMaxSize), item.first.pixelFormat, item.first.packing));
				for (int i = 0; i < item.second.size(); ++i) {
					osg::ref_ptr<osg::Image>& img = item.second.at(i);
					const int index = ta->addImage(img);
					if (index != -1) {
						item.second.erase(item.second.begin() + i);
						i--;
					}
				}
				if (ta->_texture.valid()) {
					textureAtlases.push_back(ta);
				}
			}
		}
	}
	double textureMaxSize;
	TextureVisitor* tv;
	std::vector<TextureAtlas*> textureAtlases;
	~TextureOptimizer() {
		delete tv;
		for (const auto& textureAtlas : textureAtlases) {
			delete textureAtlas;
		}
	}
	TextureOptimizer& operator=(const TextureOptimizer& other) {
		if (this != &other) {
			delete tv;
			for (const auto& textureAtlas : textureAtlases) {
				delete textureAtlas;
			}
			tv = new TextureVisitor(*(other.tv));
			textureAtlases = copyTextureAtlases(other.textureAtlases);
		}
		return *this;
	}
private:
	static std::vector<TextureAtlas*> copyTextureAtlases(const std::vector<TextureAtlas*>& source) {
		std::vector<TextureAtlas*> result;
		std::transform(source.begin(), source.end(), std::back_inserter(result),
			[](const TextureAtlas* atlas) { return new TextureAtlas(*atlas); });
		return result;
	}
};

#endif // !OSG_GIS_PLUGINS_TEXTURE_OPTIMIZIER_H
