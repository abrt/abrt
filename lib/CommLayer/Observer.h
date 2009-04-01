class CObserver {
    public:
        //CObserver();
        virtual ~CObserver() {}
        virtual void Update(const std::string& pMessage) = 0;
};
