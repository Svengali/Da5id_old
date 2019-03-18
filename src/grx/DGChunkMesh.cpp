

#include "pch_da5id.h"

#include "vox/vox.h"









static i32 s_triangleCount = 0;


class DGChunkMesh : public vox::ChunkMesh
{
public:
	DGChunkMesh()
	{
	}

	enum Faces
	{
		ePX = 1 << 0,
		eNX = 1 << 1,
		ePY = 1 << 2,
		eNY = 1 << 3,
		ePZ = 1 << 4,
		eNZ = 1 << 5,
	};

	void pushSingleCube( Faces faces, std::vector<float> *const pPos, std::vector<float> *const pAttr, std::vector<u16> *const pInd, const cb::Vec3 pos )
	{
		static const f32 N = 0;
		static const f32 P = 1;

		static const f32 positions[] = {
			// Right
			P, N, P,
			P, N, N,
			P, P, P,
			P, P, N,

			// Left
			N, N, N,
			N, N, P,
			N, P, N,
			N, P, P,

			// Top
			N, P, P,
			P, P, P,
			N, P, N,
			P, P, N,

			// Bottom
			N, N, N,
			P, N, N,
			N, N, P,
			P, N, P,

			// Near
			N, N, P,
			P, N, P,
			N, P, P,
			P, P, P,

			// Far
			P, N, N,
			N, N, N,
			P, P, N,
			N, P, N,
		};

		static const f32 attr[] = {
			// Right
			P, 0, 0, 0, 0, 0, N, 0, 0, P,
			P, 0, 0, 0, 0, 0, N, 0, P, P,
			P, 0, 0, 0, 0, 0, N, 0, 0, 0,
			P, 0, 0, 0, 0, 0, N, 0, P, 0,

			// Left
			N, 0, 0, 0, 0, 0, P, 0, 0, P,
			N, 0, 0, 0, 0, 0, P, 0, P, P,
			N, 0, 0, 0, 0, 0, P, 0, 0, 0,
			N, 0, 0, 0, 0, 0, P, 0, P, 0,

			// Top
			0, P, 0, 0, P, 0, 0, 0, 0, P,
			0, P, 0, 0, P, 0, 0, 0, P, P,
			0, P, 0, 0, P, 0, 0, 0, 0, 0,
			0, P, 0, 0, P, 0, 0, 0, P, 0,

			// Bottom
			0, N, 0, 0, P, 0, 0, 0, 0, P,
			0, N, 0, 0, P, 0, 0, 0, P, P,
			0, N, 0, 0, P, 0, 0, 0, 0, 0,
			0, N, 0, 0, P, 0, 0, 0, P, 0,

			// Near
			0, 0, P, 0, P, 0, 0, 0, 0, P,
			0, 0, P, 0, P, 0, 0, 0, P, P,
			0, 0, P, 0, P, 0, 0, 0, 0, 0,
			0, 0, P, 0, P, 0, 0, 0, P, 0,

			// Far
			0, 0, N, 0, N, 0, 0, 0, 0, P,
			0, 0, N, 0, N, 0, 0, 0, P, P,
			0, 0, N, 0, N, 0, 0, 0, 0, 0,
			0, 0, N, 0, N, 0, 0, 0, P, 0,
		};

		static const u16 indices[] = {
			0, 1, 2, 3, 2, 1,
			4, 5, 6, 7, 6, 5,
			8, 9, 10, 11, 10, 9,
			12, 13, 14, 15, 14, 13,
			16, 17, 18, 19, 18, 17,
			20, 21, 22, 23, 22, 21,
		};


		u32 curFace = 1 << 0;

		const u32 posStride = 3 * 4;
		const u32 attrStride = 10 * 4;
		const u32 indexStride = 6;

		for( i32 iFace = 0; iFace < 6; ++iFace )
		{
			if( curFace & faces )
			{
				const i32 posStart = posStride * iFace;
				const i32 posEnd = posStart + posStride;

				const i32 attrStart = attrStride * iFace;
				const i32 attrEnd = attrStart + attrStride;

				//const i32 indexStart = indexStride * i;
				//const i32 indexEnd = indexStart + indexStride;


				for( i32 iAttr = attrStart; iAttr < attrEnd; ++iAttr )
				{
					pAttr->push_back( attr[iAttr] );
				}

				const i32 indexStart = pPos->size() / 3;
				const i32 otherIndexStart = pInd->size();

				s_triangleCount += 2;

				for( i32 iIndex = 0; iIndex < 6; ++iIndex )
				{
					pInd->push_back( indices[iIndex] + indexStart );
				}

				for( i32 iPos = posStart; iPos < posEnd; ++iPos )
				{
					pPos->push_back( positions[iPos] + pos[iPos % 3] );
				}
			}

			curFace <<= 1;
		}




	}



	void cube(
		vox::CubitArr * const pCubit,
		std::vector<float> * const pPositions,
		std::vector<float> * const pAttr,
		std::vector<u16> * const pIndices,
		const vox::LPos pos,
		const cb::Vec3 worldPos,
		const u16 posX,
		const u16 negX,
		const u16 posY,
		const u16 negY,
		const u16 posZ,
		const u16 negZ
	)
	{
		const i32 index = pCubit->m_arr.index( pos );

		const auto cur = pCubit->m_arr.m_arr[index];

		const auto l = cb::Vec3( pos.x, pos.y, pos.z );

		//If its air, skip
		if( !cur ) return;

		Faces faces = (Faces)0;

		faces = cast<Faces>( faces | ( Faces::ePX * cast<int>( true && cur != posX ) ) );
		faces = cast<Faces>( faces | ( Faces::eNX * cast<int>( true && cur != negX ) ) );

		faces = cast<Faces>( faces | ( Faces::ePY * cast<int>( true && cur != posY ) ) );
		faces = cast<Faces>( faces | ( Faces::eNY * cast<int>( true && cur != negY ) ) );

		faces = cast<Faces>( faces | ( Faces::ePZ * cast<int>( true && cur != posZ ) ) );
		faces = cast<Faces>( faces | ( Faces::eNZ * cast<int>( true && cur != negZ ) ) );

		const auto h = l + cb::Vec3( 1, 1, 1 );

		if( faces )
		{
			int dummy = 0;
		}

		if( faces )
		{
			const auto startIndex = cast<u16>( pPositions->size() );

			pushSingleCube( faces, pPositions, pAttr, pIndices, cb::Vec3( worldPos.x + pos.x, worldPos.y + pos.y, worldPos.z + pos.z ) );
		}

	}


	//*
	i32 fill( vox::Plane<vox::Cubit> * pPlane, vox::CubitArr * const pCubit, const vox::CPos chunkPos, const cb::Vec3 worldPos )
	{
		std::vector<float> positions;
		//std::vector<float> attributes;
		std::vector<float> attr;
		std::vector<u16> indices;

		//material = gr::StockMaterials::get().get_checkerboard();
		//static_aabb = gr::AABB( gr::vec3( -1000 ) + worldPos, gr::vec3( 1000 ) + worldPos );

		const auto uv00 = cb::Vec2( 0, 0 );
		const auto uv01 = cb::Vec2( 0, 1 );
		const auto uv10 = cb::Vec2( 1, 0 );
		const auto uv11 = cb::Vec2( 1, 1 );

		for( i32 z = 1; z < pCubit->k_edgeSize.size - 1; ++z )
		{
			for( i32 y = 1; y < pCubit->k_edgeSize.size - 1; ++y )
			{
				for( i32 x = 1; x < pCubit->k_edgeSize.size - 1; ++x )
				{
					const auto pos = vox::LPos( x, y, z );

					const u16 posX = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 1, pos.y + 0, pos.z + 0 )];
					const u16 negX = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x - 1, pos.y + 0, pos.z + 0 )];
					const u16 posY = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 1, pos.z + 0 )];
					const u16 negY = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y - 1, pos.z + 0 )];
					const u16 posZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 0, pos.z + 1 )];
					const u16 negZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 0, pos.z - 1 )];

					cube( pCubit, &positions, &attr, &indices, pos, worldPos, posX, negX, posY, negY, posZ, negZ );

				}
			}
		}

		const i32 zSmall = 0;
		const i32 zBig = pCubit->k_edgeSize.size - 1;

		const auto vPosX = cb::Vec3i( 1, 0, 0 );
		const auto vNegX = cb::Vec3i( -1, 0, 0 );
		const auto vPosY = cb::Vec3i( 0, 1, 0 );
		const auto vNegY = cb::Vec3i( 0, -1, 0 );
		const auto vPosZ = cb::Vec3i( 0, 0, 1 );
		const auto vNegZ = cb::Vec3i( 0, 0, -1 );

		for( i32 y = 0; y < pCubit->k_edgeSize.size; ++y )
		{
			for( i32 x = 0; x < pCubit->k_edgeSize.size; ++x )
			{
				{
					const auto lPos = vox::LPos( x, y, zSmall );
					const auto gPos = chunkPos + lPos;

					const u16 posX = pPlane->get_slow( gPos + vPosX ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 1, lPos.y + 0, lPos.z + 0 )];
					const u16 negX = pPlane->get_slow( gPos + vNegX ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x - 1, lPos.y + 0, lPos.z + 0 )];
					const u16 posY = pPlane->get_slow( gPos + vPosY ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 1, lPos.z + 0 )];
					const u16 negY = pPlane->get_slow( gPos + vNegY ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y - 1, lPos.z + 0 )];
					const u16 posZ = pPlane->get_slow( gPos + vPosZ ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 0, lPos.z + 1 )];
					const u16 negZ = pPlane->get_slow( gPos + vNegZ ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 0, lPos.z - 1 )];

					cube( pCubit, &positions, &attr, &indices, lPos, worldPos, posX, negX, posY, negY, posZ, negZ );
				}

				{
					const auto lPos = vox::LPos( x, y, zBig );
					const auto gPos = chunkPos + lPos;

					const u16 posX = pPlane->get_slow( gPos + vPosX ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 1, lPos.y + 0, lPos.z + 0 )];
					const u16 negX = pPlane->get_slow( gPos + vNegX ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x - 1, lPos.y + 0, lPos.z + 0 )];
					const u16 posY = pPlane->get_slow( gPos + vPosY ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 1, lPos.z + 0 )];
					const u16 negY = pPlane->get_slow( gPos + vNegY ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y - 1, lPos.z + 0 )];
					const u16 posZ = pPlane->get_slow( gPos + vPosZ ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 0, lPos.z + 1 )];
					const u16 negZ = pPlane->get_slow( gPos + vNegZ ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 0, lPos.z - 1 )];

					cube( pCubit, &positions, &attr, &indices, lPos, worldPos, posX, negX, posY, negY, posZ, negZ );
				}


			}
		}

		const i32 ySmall = 0;
		const i32 yBig = pCubit->k_edgeSize.size - 1;

		for( i32 z = 1; z < pCubit->k_edgeSize.size - 1; ++z )
		{
			for( i32 x = 0; x < pCubit->k_edgeSize.size - 0; ++x )
			{
				{
					const auto lPos = vox::LPos( x, ySmall, z );
					const auto gPos = chunkPos + lPos;

					const u16 posX = pPlane->get_slow( gPos + vPosX ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 1, lPos.y + 0, lPos.z + 0 )];
					const u16 negX = pPlane->get_slow( gPos + vNegX ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x - 1, lPos.y + 0, lPos.z + 0 )];
					const u16 posY = pPlane->get_slow( gPos + vPosY ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 1, lPos.z + 0 )];
					const u16 negY = pPlane->get_slow( gPos + vNegY ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y - 1, lPos.z + 0 )];
					const u16 posZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 0, lPos.z + 1 )];
					const u16 negZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 0, lPos.z - 1 )];

					cube( pCubit, &positions, &attr, &indices, lPos, worldPos, posX, negX, posY, negY, posZ, negZ );
				}

				{
					const auto lPos = vox::LPos( x, yBig, z );
					const auto gPos = chunkPos + lPos;

					const u16 posX = pPlane->get_slow( gPos + vPosX ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 1, lPos.y + 0, lPos.z + 0 )];
					const u16 negX = pPlane->get_slow( gPos + vNegX ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x - 1, lPos.y + 0, lPos.z + 0 )];
					const u16 posY = pPlane->get_slow( gPos + vPosY ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 1, lPos.z + 0 )];
					const u16 negY = pPlane->get_slow( gPos + vNegY ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y - 1, lPos.z + 0 )];
					const u16 posZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 0, lPos.z + 1 )];
					const u16 negZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 0, lPos.z - 1 )];

					cube( pCubit, &positions, &attr, &indices, lPos, worldPos, posX, negX, posY, negY, posZ, negZ );
				}


			}
		}



		const i32 xSmall = 0;
		const i32 xBig = pCubit->k_edgeSize.size - 1;

		for( i32 z = 1; z < pCubit->k_edgeSize.size - 1; ++z )
		{
			for( i32 y = 1; y < pCubit->k_edgeSize.size - 1; ++y )
			{
				{
					const auto lPos = vox::LPos( xSmall, y, z );
					const auto gPos = chunkPos + lPos;

					const u16 posX = pPlane->get_slow( gPos + vPosX ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 1, lPos.y + 0, lPos.z + 0 )];
					const u16 negX = pPlane->get_slow( gPos + vNegX ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x - 1, lPos.y + 0, lPos.z + 0 )];
					const u16 posY = pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 1, lPos.z + 0 )];
					const u16 negY = pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y - 1, lPos.z + 0 )];
					const u16 posZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 0, lPos.z + 1 )];
					const u16 negZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 0, lPos.z - 1 )];

					cube( pCubit, &positions, &attr, &indices, lPos, worldPos, posX, negX, posY, negY, posZ, negZ );
				}

				{
					const auto lPos = vox::LPos( xBig, y, z );
					const auto gPos = chunkPos + lPos;

					const u16 posX = pPlane->get_slow( gPos + vPosX ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 1, lPos.y + 0, lPos.z + 0 )];
					const u16 negX = pPlane->get_slow( gPos + vNegX ); //pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x - 1, lPos.y + 0, lPos.z + 0 )];
					const u16 posY = pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 1, lPos.z + 0 )];
					const u16 negY = pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y - 1, lPos.z + 0 )];
					const u16 posZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 0, lPos.z + 1 )];
					const u16 negZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( lPos.x + 0, lPos.y + 0, lPos.z - 1 )];

					cube( pCubit, &positions, &attr, &indices, lPos, worldPos, posX, negX, posY, negY, posZ, negZ );
				}


			}
		}




		//*
		{
			auto negX = chunkPos;
			negX += vox::CPos( -1, 0, 0 );

			const auto chunkNegXOpt = pPlane->get( negX );

			const i32 x = 0;

			if( chunkNegXOpt.has_value() )
			{
				const auto chunkNegX = chunkNegXOpt.value();

				for( i32 z = 0; z < pCubit->k_edgeSize.size - 1; ++z )
				{
					for( i32 y = 0; y < pCubit->k_edgeSize.size - 1; ++y )
					{
						const auto pos = vox::LPos( x, y, z );

						const u16 posX = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 1, pos.y + 0, pos.z + 0 )];
						//const u16 negX = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x - 1, pos.y + 0, pos.z + 0 )];
						const u16 posY = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 1, pos.z + 0 )];
						const u16 negY = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y - 1, pos.z + 0 )];
						const u16 posZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 0, pos.z + 1 )];
						const u16 negZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 0, pos.z - 1 )];

						auto otherPos = vox::LPos( 15, y, z );

						const u16 negX = chunkNegX->get_slow( otherPos );

						cube( pCubit, &positions, &attr, &indices, pos, worldPos, posX, negX, posY, negY, posZ, negZ );
					}
				}
			}
			else
			{
				for( i32 z = 0; z < pCubit->k_edgeSize.size - 1; ++z )
				{
					for( i32 y = 0; y < pCubit->k_edgeSize.size - 1; ++y )
					{
						const auto pos = vox::LPos( x, y, z );

						const u16 posX = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 1, pos.y + 0, pos.z + 0 )];
						//const u16 negX = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x - 1, pos.y + 0, pos.z + 0 )];
						const u16 posY = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 1, pos.z + 0 )];
						const u16 negY = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y - 1, pos.z + 0 )];
						const u16 posZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 0, pos.z + 1 )];
						const u16 negZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 0, pos.z - 1 )];

						const u16 negX = 0;

						cube( pCubit, &positions, &attr, &indices, pos, worldPos, posX, negX, posY, negY, posZ, negZ );
					}
				}
			}
		}

		{
			auto posX = chunkPos;
			posX += vox::CPos( 1, 0, 0 );

			const auto chunkNegXOpt = pPlane->get( posX );

			const i32 x = 15;

			if( chunkNegXOpt.has_value() )
			{
				const auto chunkNegX = chunkNegXOpt.value();

				for( i32 z = 0; z < pCubit->k_edgeSize.size - 1; ++z )
				{
					for( i32 y = 0; y < pCubit->k_edgeSize.size - 1; ++y )
					{
						const auto pos = vox::LPos( x, y, z );

						//const u16 posX = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 1, pos.y + 0, pos.z + 0 )];
						const u16 negX = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x - 1, pos.y + 0, pos.z + 0 )];
						const u16 posY = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 1, pos.z + 0 )];
						const u16 negY = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y - 1, pos.z + 0 )];
						const u16 posZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 0, pos.z + 1 )];
						const u16 negZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 0, pos.z - 1 )];

						auto otherPos = vox::LPos( 0, y, z );

						const u16 posX = chunkNegX->get_slow( otherPos );

						cube( pCubit, &positions, &attr, &indices, pos, worldPos, posX, negX, posY, negY, posZ, negZ );
					}
				}
			}
			else
			{
				for( i32 z = 0; z < pCubit->k_edgeSize.size - 1; ++z )
				{
					for( i32 y = 0; y < pCubit->k_edgeSize.size - 1; ++y )
					{
						const auto pos = vox::LPos( x, y, z );

						const u16 posX = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 1, pos.y + 0, pos.z + 0 )];
						//const u16 negX = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x - 1, pos.y + 0, pos.z + 0 )];
						const u16 posY = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 1, pos.z + 0 )];
						const u16 negY = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y - 1, pos.z + 0 )];
						const u16 posZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 0, pos.z + 1 )];
						const u16 negZ = pCubit->m_arr.m_arr[pCubit->m_arr.index( pos.x + 0, pos.y + 0, pos.z - 1 )];

						const u16 negX = 0;

						cube( pCubit, &positions, &attr, &indices, pos, worldPos, posX, negX, posY, negY, posZ, negZ );
					}
				}
			}
		}
		//* /

		if(positions.size() == 0)
			return 0;

		/*
		Vulkan::BufferCreateInfo info = {};
		info.size = positions.size() * sizeof( float );
		info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		info.domain = Vulkan::BufferDomain::Device;
		vbo_position = pDev->create_buffer( info, positions.data() );

		info.size = attr.size() * sizeof( float );
		vbo_attributes = pDev->create_buffer( info, attr.data() );

		attributes[cast<i32>( gr::MeshAttribute::Position )].offset = 0;
		attributes[cast<i32>( gr::MeshAttribute::Position )].format = VK_FORMAT_R32G32B32_SFLOAT;

		attributes[cast<i32>( gr::MeshAttribute::Normal )].offset = 0;
		attributes[cast<i32>( gr::MeshAttribute::Normal )].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributes[cast<i32>( gr::MeshAttribute::Tangent )].offset = 4;
		attributes[cast<i32>( gr::MeshAttribute::Tangent )].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributes[cast<i32>( gr::MeshAttribute::UV )].offset = 8;
		attributes[cast<i32>( gr::MeshAttribute::UV )].format = VK_FORMAT_R32G32_SFLOAT;
		position_stride = sizeof( gr::vec3 );
		attribute_stride = sizeof( float ) * 10;

		info.size = indices.size() * sizeof( uint16_t );
		info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		ibo = pDev->create_buffer( info, indices.data() );
		ibo_offset = 0;
		index_type = VK_INDEX_TYPE_UINT16;
		count = indices.size();
		topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		bake();
		*/

		return positions.size();
	}
	//*/
};


